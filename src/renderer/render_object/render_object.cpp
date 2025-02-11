//
// Created by William on 2025-01-24.
//

#include "render_object.h"

#include <extern/stb/stb_image.h>
#include <extern/fastgltf/include/fastgltf/core.hpp>
#include <extern/fastgltf/include/fastgltf/types.hpp>
#include <extern/fastgltf/include/fastgltf/tools.hpp>
#include <extern/fmt/include/fmt/format.h>
#include <vulkan/vulkan_core.h>

#include "src/renderer/vulkan_context.h"
#include "render_object_constants.h"
#include "../../core/identifier/identifier_manager.h"
#include "src/core/game_object/game_object.h"
#include "src/util/file.h"

namespace will_engine
{
RenderObject::RenderObject(const std::filesystem::path& gltfFilepath, ResourceManager& resourceManager, const int64_t renderObjectId)
    : gltfFilepath(gltfFilepath), resourceManager(resourceManager)
{
    freeInstanceIndices.reserve(10);
    for (int32_t i = 0; i < 10; ++i) { freeInstanceIndices.insert(i); }
    currentInstanceCount = freeInstanceIndices.size();
    dirty();

    if (!parseGltf(gltfFilepath)) { return; }
    generateBuffers();
}

RenderObject::~RenderObject()
{
    for (auto& image : images) {
        if (image.image == resourceManager.getErrorCheckerboardImage().image || image.image == resourceManager.getWhiteImage().image) {
            //dont destroy the default images
            continue;
        }

        resourceManager.destroyImage(image);
    }

    for (const auto& sampler : samplers) {
        if (sampler == resourceManager.getDefaultSamplerNearest() || sampler == resourceManager.getDefaultSamplerLinear()) {
            //dont destroy the default samplers
            continue;
        }

        resourceManager.destroySampler(sampler);
    }

    resourceManager.destroyBuffer(indexBuffer);
    resourceManager.destroyBuffer(vertexBuffer);

    resourceManager.destroyBuffer(materialBuffer);

    resourceManager.destroyBuffer(meshBoundsBuffer);

    for (int i = 0; i < FRAME_OVERLAP; ++i) {
        resourceManager.destroyBuffer(drawIndirectBuffers[i]);
        resourceManager.destroyBuffer(addressBuffers[i]);
        resourceManager.destroyBuffer(modelMatrixBuffers[i]);
        resourceManager.destroyBuffer(cullingAddressBuffers[i]);
        resourceManager.destroyBuffer(boundingSphereIndicesBuffers[i]);
    }

    resourceManager.destroyDescriptorBuffer(addressesDescriptorBuffer);
    resourceManager.destroyDescriptorBuffer(frustumCullingDescriptorBuffer);
    resourceManager.destroyDescriptorBuffer(textureDescriptorBuffer);
}

void RenderObject::update(const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    if (framesToUpdate <= 0) { return; }
    AllocatedBuffer& currentInstanceBuffer = modelMatrixBuffers[currentFrameOverlap];
    const AllocatedBuffer& previousInstanceBuffer = modelMatrixBuffers[previousFrameOverlap];

    // sizes don't match, need to recreate the buffer
    if (currentInstanceBuffer.info.size != currentInstanceCount * sizeof(InstanceData)) {
        const AllocatedBuffer newInstanceBuffer = resourceManager.createHostRandomBuffer(currentInstanceCount * sizeof(InstanceData),
                                                                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);


        // copy data from previous frame, as it is the most up to date
        const size_t sizeToCopy = glm::min(currentInstanceCount * sizeof(InstanceData), previousInstanceBuffer.info.size);
        if (sizeToCopy > 0) {
            resourceManager.copyBuffer(previousInstanceBuffer, newInstanceBuffer, sizeToCopy);
        }
        resourceManager.destroyBuffer(currentInstanceBuffer);
        currentInstanceBuffer = newInstanceBuffer;

        const VkDeviceAddress instanceBufferAddress = resourceManager.getBufferAddress(currentInstanceBuffer);
        memcpy(static_cast<char*>(addressBuffers[currentFrameOverlap].info.pMappedData) + sizeof(VkDeviceAddress), &instanceBufferAddress, sizeof(VkDeviceAddress));
    }

    // instance records could be stale,  need to ensure that it cleared/invalidated on GPU
    for (const uint32_t freeIndex : freeInstanceIndices) {
        auto targetModel = reinterpret_cast<InstanceData*>(static_cast<char*>(currentInstanceBuffer.info.pMappedData) + freeIndex * sizeof(InstanceData));

        assert(reinterpret_cast<uintptr_t>(targetModel) % alignof(InstanceData) == 0 && "Misaligned instance data access");

        targetModel->previousModelMatrix = glm::identity<glm::mat4>();
        targetModel->currentModelMatrix = glm::identity<glm::mat4>();
        targetModel->flags = glm::vec4(0.0f);
    }

    AllocatedBuffer& currentDrawIndirectBuffer = drawIndirectBuffers[currentFrameOverlap];
    if (currentDrawIndirectBuffer.info.size != drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand)) {
        resourceManager.destroyBuffer(currentDrawIndirectBuffer);
        AllocatedBuffer indirectStaging = resourceManager.createStagingBuffer(drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        memcpy(indirectStaging.info.pMappedData, drawCommands.data(), drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        currentDrawIndirectBuffer = resourceManager.createDeviceBuffer(drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

        resourceManager.copyBuffer(indirectStaging, currentDrawIndirectBuffer, drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        resourceManager.destroyBuffer(indirectStaging);
    }

    AllocatedBuffer& currentBoundingSphereIndicesBuffer = boundingSphereIndicesBuffers[currentFrameOverlap];
    resourceManager.destroyBuffer(currentBoundingSphereIndicesBuffer);
    AllocatedBuffer stagingBoundingSphereIndicesBuffer = resourceManager.createStagingBuffer(boundingSphereIndices.size() * sizeof(uint32_t));
    memcpy(stagingBoundingSphereIndicesBuffer.info.pMappedData, boundingSphereIndices.data(), boundingSphereIndices.size() * sizeof(uint32_t));
    currentBoundingSphereIndicesBuffer = resourceManager.createDeviceBuffer(boundingSphereIndices.size() * sizeof(uint32_t));
    resourceManager.copyBuffer(stagingBoundingSphereIndicesBuffer, currentBoundingSphereIndicesBuffer, boundingSphereIndices.size() * sizeof(uint32_t));
    resourceManager.destroyBuffer(stagingBoundingSphereIndicesBuffer);


    const FrustumCullingBuffers cullingAddresses{
        .meshBoundsBuffer = resourceManager.getBufferAddress(meshBoundsBuffer),
        .commandBuffer = resourceManager.getBufferAddress(currentDrawIndirectBuffer),
        .commandBufferCount = static_cast<uint32_t>(drawCommands.size()),
        .modelMatrixBuffer = resourceManager.getBufferAddress(currentInstanceBuffer),
        .meshIndicesBuffer = resourceManager.getBufferAddress(currentBoundingSphereIndicesBuffer),
        .padding = {},
    };

    const auto currentCullingAddressBuffers = cullingAddressBuffers[currentFrameOverlap];
    AllocatedBuffer stagingCullingAddressesBuffer = resourceManager.createStagingBuffer(sizeof(FrustumCullingBuffers));
    memcpy(stagingCullingAddressesBuffer.info.pMappedData, &cullingAddresses, sizeof(FrustumCullingBuffers));
    resourceManager.copyBuffer(stagingCullingAddressesBuffer, currentCullingAddressBuffers, sizeof(FrustumCullingBuffers));
    resourceManager.destroyBuffer(stagingCullingAddressesBuffer);

    framesToUpdate--;
}

GameObject* RenderObject::generateGameObject(const std::string& gameObjectName)
{
    // get number of meshes in the entire model
    uint32_t instanceCount{0};
    for (const RenderNode& renderNode : renderNodes) {
        if (renderNode.meshIndex != -1) {
            instanceCount++;
        }
    }
    auto* superRoot = new GameObject(gameObjectName);
    for (const int32_t rootNode : topNodes) {
        recursiveGenerateGameObject(renderNodes[rootNode], superRoot);
    }

    //uploadCullingBufferData();
    dirty();
    return superRoot;
}

void RenderObject::recursiveGenerateGameObject(const RenderNode& renderNode, GameObject* parent)
{
    const auto gameObject = new GameObject();

    if (renderNode.meshIndex != -1) {
        // InstanceIndex is used to know which model matrix to use form the model matrix array
        // All primitives in a mesh use the same model matrix
        const std::vector<Primitive>& meshPrimitives = meshes[renderNode.meshIndex].primitives;
        gameObject->setName(meshes[renderNode.meshIndex].name);
        drawCommands.reserve(drawCommands.size() + meshPrimitives.size());

        const int32_t instanceIndex = getFreeInstanceIndex();

        for (const Primitive primitive : meshPrimitives) {
            drawCommands.emplace_back();
            VkDrawIndexedIndirectCommand& indirectData = drawCommands.back();
            indirectData.firstIndex = primitive.firstIndex;
            indirectData.indexCount = primitive.indexCount;
            indirectData.vertexOffset = primitive.vertexOffset;
            indirectData.instanceCount = 1;
            indirectData.firstInstance = instanceIndex;

            boundingSphereIndices.push_back(primitive.boundingSphereIndex);
        }

        gameObject->setRenderObjectReference(this, instanceIndex, renderNode.meshIndex);
    }

    gameObject->setLocalTransform(renderNode.transform);
    parent->addChild(gameObject);

    for (const auto& child : renderNode.children) {
        recursiveGenerateGameObject(*child, gameObject);
    }
}

bool RenderObject::generateMesh(GameObject* gameObject, const int32_t meshIndex)
{
    if (gameObject == nullptr) { return false; }
    if (meshIndex < 0 || meshIndex >= meshes.size()) { return false; }

    const int32_t instanceIndex = getFreeInstanceIndex();

    const std::vector<Primitive>& meshPrimitives = meshes[meshIndex].primitives;
    drawCommands.reserve(drawCommands.size() + meshPrimitives.size());
    boundingSphereIndices.reserve(boundingSphereIndices.size() + meshPrimitives.size());

    for (const Primitive primitive : meshPrimitives) {
        drawCommands.emplace_back();
        VkDrawIndexedIndirectCommand& indirectData = drawCommands.back();
        indirectData.firstIndex = primitive.firstIndex;
        indirectData.indexCount = primitive.indexCount;
        indirectData.vertexOffset = primitive.vertexOffset;
        indirectData.instanceCount = 1;
        indirectData.firstInstance = instanceIndex;

        boundingSphereIndices.push_back(primitive.boundingSphereIndex);
    }

    gameObject->setRenderObjectReference(this, instanceIndex, meshIndex);

    dirty();
    return true;
}

void RenderObject::updateInstanceData(const int32_t instanceIndex, const CurrentInstanceData& newInstanceData, const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    if (instanceIndex < 0 || instanceIndex >= currentInstanceCount) {
        assert(false && "Instance index out of bounds");
    }

    const AllocatedBuffer& previousFrameModelMatrix = modelMatrixBuffers[previousFrameOverlap];
    const AllocatedBuffer& currentFrameModelMatrix = modelMatrixBuffers[currentFrameOverlap];

    auto prevModel = reinterpret_cast<InstanceData*>(static_cast<char*>(previousFrameModelMatrix.info.pMappedData) + instanceIndex * sizeof(InstanceData));
    auto currentModel = reinterpret_cast<InstanceData*>(static_cast<char*>(currentFrameModelMatrix.info.pMappedData) + instanceIndex * sizeof(InstanceData));

    assert(reinterpret_cast<uintptr_t>(prevModel) % alignof(InstanceData) == 0 && "Misaligned instance data access");
    assert(reinterpret_cast<uintptr_t>(currentModel) % alignof(InstanceData) == 0 && "Misaligned instance data access");

    currentModel->previousModelMatrix = prevModel->currentModelMatrix;
    currentModel->currentModelMatrix = newInstanceData.currentModelMatrix;
    currentModel->flags[0] = true; // if updateInstanceData is being called, then isActive is true.
    currentModel->flags[1] = newInstanceData.bIsVisible;
}

bool RenderObject::releaseInstanceIndex(const uint32_t instanceIndex)
{
    if (freeInstanceIndices.contains(instanceIndex)) {
        fmt::print("WARNING: Render object instructed to release instance index when it is already free");
        return false;
    }

    freeInstanceIndices.insert(instanceIndex);
    for (int32_t i = static_cast<int32_t>(drawCommands.size()) - 1; i >= 0; --i) {
        if (drawCommands[i].firstInstance == instanceIndex) {
            drawCommands.erase(drawCommands.begin() + i);
            boundingSphereIndices.erase(boundingSphereIndices.begin() + i);
        }
    }

    dirty();
    return true;
}

bool RenderObject::parseGltf(const std::filesystem::path& gltfFilepath)
{
    auto start = std::chrono::system_clock::now();

    fastgltf::Parser parser{};
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember
                                 | fastgltf::Options::AllowDouble
                                 | fastgltf::Options::LoadExternalBuffers
                                 | fastgltf::Options::LoadExternalImages;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(gltfFilepath);
    if (!static_cast<bool>(gltfFile)) { fmt::print("Failed to open glTF file ({}): {}\n", gltfFilepath.filename().string(), getErrorMessage(gltfFile.error())); }

    auto load = parser.loadGltf(gltfFile.get(), gltfFilepath.parent_path(), gltfOptions);
    if (!load) {
        fmt::print("Failed to load glTF: {}\n", to_underlying(load.error()));
        return false;
    }

    fastgltf::Asset gltf = std::move(load.get());

    samplers.reserve(gltf.samplers.size() + samplerOffset);
    samplers.push_back(this->resourceManager.getDefaultSamplerNearest());
    for (const fastgltf::Sampler& gltfSampler : gltf.samplers) {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0;

        samplerInfo.magFilter = extractFilter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = extractFilter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));


        samplerInfo.mipmapMode = extractMipMapMode(gltfSampler.minFilter.value_or(fastgltf::Filter::Linear));
        samplers.emplace_back(this->resourceManager.createSampler(samplerInfo));
    }

    assert(samplers.size() <= render_object_constants::MAX_SAMPLER_COUNT);

    images.reserve(gltf.images.size() + imageOffset);
    images.push_back(this->resourceManager.getWhiteImage());
    for (const fastgltf::Image& gltfImage : gltf.images) {
        std::optional<AllocatedImage> newImage = loadImage(gltf, gltfImage, gltfFilepath.parent_path());
        if (newImage.has_value()) {
            images.push_back(*newImage);
        } else {
            images.push_back(this->resourceManager.getErrorCheckerboardImage());
        }
    }

    assert(images.size() <= render_object_constants::MAX_IMAGES_COUNT);

    int32_t materialOffset = 1;
    // default material at 0
    materials.reserve(gltf.materials.size() + materialOffset);
    Material defaultMaterial{
        glm::vec4(1.0f),
        {0.0f, 1.0f, 0.0f, 0.0f},
        glm::vec4(0.0f),
        glm::vec4(0.0f),
        {1.0f, 0.0f, 0.0f, 0.0f}
    };
    materials.push_back(defaultMaterial);
    for (const fastgltf::Material& gltfMaterial : gltf.materials) {
        Material material = extractMaterial(gltf, gltfMaterial);
        materials.push_back(material);
    }

    meshes.reserve(gltf.meshes.size());
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        Mesh meshData{};
        meshData.name = mesh.name;
        meshData.primitives.reserve(mesh.primitives.size());
        boundingSpheres.reserve(boundingSpheres.size() + mesh.primitives.size());

        for (fastgltf::Primitive& p : mesh.primitives) {
            Primitive primitiveData{};

            std::vector<Vertex> primitiveVertices{};
            std::vector<uint32_t> primitiveIndices{};

            glm::uint32_t primitiveMaterialIndex{0};

            if (p.materialIndex.has_value()) {
                primitiveMaterialIndex = p.materialIndex.value() + materialOffset;
                primitiveData.bHasTransparent = (static_cast<MaterialType>(materials[primitiveMaterialIndex].alphaCutoff.y) == MaterialType::TRANSPARENT);
            }

            // INDICES
            const fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
            primitiveIndices.reserve(indexAccessor.count);

            fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, [&](const std::uint32_t idx) {
                primitiveIndices.push_back(idx);
            });

            // POSITION (REQUIRED)
            const fastgltf::Attribute* positionIt = p.findAttribute("POSITION");
            const fastgltf::Accessor& posAccessor = gltf.accessors[positionIt->accessorIndex];
            primitiveVertices.resize(posAccessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, posAccessor, [&](fastgltf::math::fvec3 v, const size_t index) {
                Vertex newVertex{};
                newVertex.position = {v.x(), v.y(), v.z()};
                newVertex.materialIndex = primitiveMaterialIndex;
                primitiveVertices[index] = newVertex;
            });


            // NORMALS
            const fastgltf::Attribute* normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex], [&](fastgltf::math::fvec3 n, const size_t index) {
                    primitiveVertices[index].normal = {n.x(), n.y(), n.z()};
                });
            }

            // UV
            const fastgltf::Attribute* uvs = p.findAttribute("TEXCOORD_0");
            if (uvs != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uvs->accessorIndex], [&](fastgltf::math::fvec2 uv, const size_t index) {
                    primitiveVertices[index].uv = {uv.x(), uv.y()};
                });
            }

            // VERTEX COLOR
            const fastgltf::Attribute* colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex], [&](fastgltf::math::fvec4 color, const size_t index) {
                    primitiveVertices[index].color = {
                        color.x(), color.y(), color.z(), color.w()
                    };
                });
            }

            boundingSpheres.emplace_back(primitiveVertices);

            primitiveData.firstIndex = static_cast<uint32_t>(indices.size());
            primitiveData.vertexOffset = static_cast<int32_t>(vertices.size());
            primitiveData.indexCount = static_cast<uint32_t>(primitiveIndices.size());
            primitiveData.boundingSphereIndex = static_cast<uint32_t>(boundingSpheres.size() - 1);

            vertices.insert(vertices.end(), primitiveVertices.begin(), primitiveVertices.end());
            indices.insert(indices.end(), primitiveIndices.begin(), primitiveIndices.end());
            meshData.primitives.push_back(primitiveData);
        }

        meshes.push_back(meshData);
    }

    for (const fastgltf::Node& node : gltf.nodes) {
        RenderNode renderNode{};
        if (node.meshIndex.has_value()) {
            renderNode.meshIndex = static_cast<int>(*node.meshIndex);
        }

        std::visit(
            fastgltf::visitor{
                [&](fastgltf::math::fmat4x4 matrix) {
                    glm::mat4 glmMatrix;
                    for (int i = 0; i < 4; ++i) {
                        for (int j = 0; j < 4; ++j) {
                            glmMatrix[i][j] = matrix[i][j];
                        }
                    }

                    const auto translation = glm::vec3(glmMatrix[3]);
                    const glm::vec3 scale = glm::vec3(
                        glm::length(glm::vec3(glmMatrix[0])),
                        glm::length(glm::vec3(glmMatrix[1])),
                        glm::length(glm::vec3(glmMatrix[2]))
                    );
                    const glm::quat rotation = glm::quat_cast(glmMatrix);

                    renderNode.transform = Transform(translation, rotation, scale);
                },
                [&](fastgltf::TRS transform) {
                    const glm::vec3 translation{transform.translation[0], transform.translation[1], transform.translation[2]};
                    const glm::quat quaternionRotation{transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]};
                    const glm::vec3 scale{transform.scale[0], transform.scale[1], transform.scale[2]};

                    renderNode.transform = Transform(translation, quaternionRotation, scale);
                }
            }
            , node.transform
        );

        renderNodes.push_back(renderNode);
    }

    for (int i = 0; i < gltf.nodes.size(); i++) {
        for (std::size_t& child : gltf.nodes[i].children) {
            renderNodes[i].children.push_back(&renderNodes[child]);
            renderNodes[child].parent = &renderNodes[i];
        }
    }

    for (int i = 0; i < renderNodes.size(); i++) {
        if (renderNodes[i].parent == nullptr) {
            topNodes.push_back(i);
        }
    }

    uint32_t instanceCount{0};
    for (const RenderNode& renderNode : renderNodes) {
        if (renderNode.meshIndex != -1) {
            instanceCount++;
        }
    }

    size_t primitiveCount{0};
    for (const Mesh& mesh : meshes) {
        primitiveCount += mesh.primitives.size();
    }
    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    float time = static_cast<float>(elapsed.count()) / 1000000.0f;
    fmt::print("GLTF: {} | Sampl: {} | Imag: {} | Mats: {} | Mesh: {} | Prim: {} | Inst: {} | in {}ms\n", file::getFileName(gltfFilepath.filename().string().c_str()), samplers.size() - samplerOffset,
               images.size() - imageOffset, materials.size() - materialOffset, meshes.size(), primitiveCount, instanceCount, time);
    return true;
}

std::optional<AllocatedImage> RenderObject::loadImage(const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& parentFolder) const
{
    AllocatedImage newImage{};

    int width{}, height{}, nrChannels{};

    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](const fastgltf::sources::URI& fileName) {
                assert(fileName.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(fileName.uri.isLocalPath()); // We're only capable of loading
                // local files.
                const std::wstring widePath(fileName.uri.path().begin(), fileName.uri.path().end());
                const std::filesystem::path fullPath = parentFolder / widePath;

                //std::string extension = getFileExtension(fullpath);
                /*if (isKTXFile(extension)) {
                    ktxTexture* kTexture;
                    KTX_error_code ktxresult;

                    ktxresult = ktxTexture_CreateFromNamedFile(
                        fullpath.c_str(),
                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                        &kTexture);


                    if (ktxresult == KTX_SUCCESS) {
                        VkImageFormatProperties formatProperties;
                        VkResult result = vkGetPhysicalDeviceImageFormatProperties(engine->_physicalDevice
                            , ktxTexture_GetVkFormat(kTexture), VK_IMAGE_TYPE_2D
                            , VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, &formatProperties);
                        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
                            fmt::print("Image found with format not supported\n");
                            VkExtent3D imagesize;
                            imagesize.width = 1;
                            imagesize.height = 1;
                            imagesize.depth = 1;
                            unsigned char data[4] = { 255, 0, 255, 1 };
                            newImage = engine->_resourceConstructor->create_image(data, 4, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
                        }
                        else {
                            unsigned char* data = (unsigned char*)ktxTexture_GetData(kTexture);
                            VkExtent3D imageExtents{};
                            imageExtents.width = kTexture->baseWidth;
                            imageExtents.height = kTexture->baseHeight;
                            imageExtents.depth = 1;
                            newImage = engine->_resourceConstructor->create_image(data, kTexture->dataSize, imageExtents, ktxTexture_GetVkFormat(kTexture), VK_IMAGE_USAGE_SAMPLED_BIT, false);
                        }

                    }

                    ktxTexture_Destroy(kTexture);
                }*/

                // ReSharper disable once CppTooWideScope
                unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;
                    const size_t size = width * height * 4;
                    newImage = resourceManager.createImage(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            [&](const fastgltf::sources::Array& vector) {
                // ReSharper disable once CppTooWideScope
                unsigned char* data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                                                            static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;
                    const size_t size = width * height * 4;
                    newImage = resourceManager.createImage(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            [&](const fastgltf::sources::BufferView& view) {
                const fastgltf::BufferView& bufferView = asset.bufferViews[view.bufferViewIndex];
                const fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];
                // We only care about VectorWithMime here, because we
                // specify LoadExternalBuffers, meaning all buffers
                // are already loaded into a vector.
                std::visit(fastgltf::visitor{
                               [](auto& arg) {},
                               [&](const fastgltf::sources::Array& vector) {
                                   // ReSharper disable once CppTooWideScope
                                   unsigned char* data = stbi_load_from_memory(
                                       reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset),
                                       static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;
                                       const size_t size = width * height * 4;
                                       newImage = resourceManager.createImage(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
                                       stbi_image_free(data);
                                   }
                               }
                           }, buffer.data);
            }
        }, image.data);

    // if any of the attempts to load the data failed, we haven't written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        fmt::print("Image failed to load: {}\n", image.name.c_str());
        return {};
    } else {
        return newImage;
    }
}

Material RenderObject::extractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial)
{
    Material material = {};
    material.colorFactor = glm::vec4(
        gltfMaterial.pbrData.baseColorFactor[0]
        , gltfMaterial.pbrData.baseColorFactor[1]
        , gltfMaterial.pbrData.baseColorFactor[2]
        , gltfMaterial.pbrData.baseColorFactor[3]);

    material.metalRoughFactors.x = gltfMaterial.pbrData.metallicFactor;
    material.metalRoughFactors.y = gltfMaterial.pbrData.roughnessFactor;
    material.alphaCutoff.x = 0.5f;

    switch (gltfMaterial.alphaMode) {
        case fastgltf::AlphaMode::Opaque:
            material.alphaCutoff.x = 1.0f;
            material.alphaCutoff.y = static_cast<float>(MaterialType::OPAQUE);
            break;
        case fastgltf::AlphaMode::Blend:
            material.alphaCutoff.x = 1.0f;
            material.alphaCutoff.y = static_cast<float>(MaterialType::TRANSPARENT);
            break;
        case fastgltf::AlphaMode::Mask:
            material.alphaCutoff.x = gltfMaterial.alphaCutoff;
            material.alphaCutoff.y = static_cast<float>(MaterialType::MASK);
            break;
    }

    loadTextureIndices(gltfMaterial.pbrData.baseColorTexture, gltf, material.textureImageIndices.x, material.textureSamplerIndices.x);
    loadTextureIndices(gltfMaterial.pbrData.metallicRoughnessTexture, gltf, material.textureImageIndices.y, material.textureSamplerIndices.y);

    // image defined but no sampler and vice versa - use defaults
    // pretty rare/edge case
    if (material.textureImageIndices.x == -1 && material.textureSamplerIndices.x != -1) {
        material.textureImageIndices.x = 0;
    }
    if (material.textureSamplerIndices.x == -1 && material.textureImageIndices.x != -1) {
        material.textureSamplerIndices.x = 0;
    }
    if (material.textureImageIndices.y == -1 && material.textureSamplerIndices.y != -1) {
        material.textureImageIndices.y = 0;
    }
    if (material.textureSamplerIndices.y == -1 && material.textureImageIndices.y != -1) {
        material.textureSamplerIndices.y = 0;
    }
    return material;
}

void RenderObject::loadTextureIndices(const fastgltf::Optional<fastgltf::TextureInfo>& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex)
{
    if (!texture.has_value()) { return; }

    const size_t textureIndex = texture.value().textureIndex;
    if (gltf.textures[textureIndex].imageIndex.has_value()) {
        imageIndex = gltf.textures[textureIndex].imageIndex.value() + imageOffset;
    }

    if (gltf.textures[textureIndex].samplerIndex.has_value()) {
        samplerIndex = gltf.textures[textureIndex].samplerIndex.value() + samplerOffset;
    }
}

VkFilter RenderObject::extractFilter(const fastgltf::Filter filter)
{
    switch (filter) {
        // nearest samplers
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;

        // linear samplers
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode RenderObject::extractMipMapMode(const fastgltf::Filter filter)
{
    switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;

        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

bool RenderObject::generateBuffers()
{
    std::vector<DescriptorImageData> textureDescriptors;
    for (const VkSampler sampler : samplers) {
        textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, {.sampler = sampler}, false});
    }

    const size_t remaining = render_object_constants::MAX_SAMPLER_COUNT - samplers.size();
    for (int i = 0; i < remaining; i++) {
        textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, {}, true});
    }

    for (const AllocatedImage& image : images) {
        textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, {.imageView = image.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, false});
    }


    textureDescriptorBuffer = resourceManager.createDescriptorBufferSampler(resourceManager.getTexturesLayout(), 1);
    resourceManager.setupDescriptorBufferSampler(textureDescriptorBuffer, textureDescriptors, 0);


    AllocatedBuffer materialStaging = resourceManager.createStagingBuffer(materials.size() * sizeof(Material));
    memcpy(materialStaging.info.pMappedData, materials.data(), materials.size() * sizeof(Material));
    materialBuffer = resourceManager.createDeviceBuffer(materials.size() * sizeof(Material));
    resourceManager.copyBuffer(materialStaging, materialBuffer, materials.size() * sizeof(Material));
    resourceManager.destroyBuffer(materialStaging);


    AllocatedBuffer vertexStaging = resourceManager.createStagingBuffer(vertices.size() * sizeof(Vertex));
    memcpy(vertexStaging.info.pMappedData, vertices.data(), vertices.size() * sizeof(Vertex));
    vertexBuffer = resourceManager.createDeviceBuffer(vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    resourceManager.copyBuffer(vertexStaging, vertexBuffer, vertices.size() * sizeof(Vertex));
    resourceManager.destroyBuffer(vertexStaging);

    AllocatedBuffer indexStaging = resourceManager.createStagingBuffer(indices.size() * sizeof(uint32_t));
    memcpy(indexStaging.info.pMappedData, indices.data(), indices.size() * sizeof(uint32_t));
    indexBuffer = resourceManager.createDeviceBuffer(indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    resourceManager.copyBuffer(indexStaging, indexBuffer, indices.size() * sizeof(uint32_t));
    resourceManager.destroyBuffer(indexStaging);

    // Addresses (Texture and Uniform model data)
    // todo: make address buffer for statics (dont need multiple)
    addressesDescriptorBuffer = resourceManager.createDescriptorBufferUniform(resourceManager.getAddressesLayout(), FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; i++) {
        constexpr size_t addressesSize = sizeof(VkDeviceAddress) * 2;
        addressBuffers[i] = resourceManager.createHostSequentialBuffer(addressesSize);
        DescriptorUniformData addressesUniformData{
            .uniformBuffer = addressBuffers[i],
            .allocSize = addressesSize,
        };
        resourceManager.setupDescriptorBufferUniform(addressesDescriptorBuffer, {addressesUniformData}, i);

        modelMatrixBuffers[i] = resourceManager.createHostRandomBuffer(currentInstanceCount * sizeof(InstanceData), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);


        const VkDeviceAddress materialBufferAddress = resourceManager.getBufferAddress(materialBuffer);
        const VkDeviceAddress instanceBufferAddress = resourceManager.getBufferAddress(modelMatrixBuffers[i]);
        const VkDeviceAddress addresses[2] = {materialBufferAddress, instanceBufferAddress};
        memcpy(addressBuffers[i].info.pMappedData, addresses, sizeof(VkDeviceAddress) * 2);
    }

    frustumCullingDescriptorBuffer = resourceManager.createDescriptorBufferUniform(resourceManager.getFrustumCullLayout(), FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; i++) {
        cullingAddressBuffers[i] = resourceManager.createDeviceBuffer(sizeof(FrustumCullingBuffers));
        const DescriptorUniformData cullingAddressesUniformData{
            .uniformBuffer = cullingAddressBuffers[i],
            .allocSize = sizeof(FrustumCullingBuffers),
        };
        resourceManager.setupDescriptorBufferUniform(frustumCullingDescriptorBuffer, {cullingAddressesUniformData}, i);
    }


    AllocatedBuffer meshBoundsStaging = resourceManager.createStagingBuffer(sizeof(BoundingSphere) * boundingSpheres.size());
    memcpy(meshBoundsStaging.info.pMappedData, boundingSpheres.data(), sizeof(BoundingSphere) * boundingSpheres.size());
    meshBoundsBuffer = resourceManager.createDeviceBuffer(sizeof(BoundingSphere) * boundingSpheres.size());
    resourceManager.copyBuffer(meshBoundsStaging, meshBoundsBuffer, sizeof(BoundingSphere) * boundingSpheres.size());
    resourceManager.destroyBuffer(meshBoundsStaging);

    return true;
}

bool RenderObject::releaseBuffers()
{
    if (currentInstanceCount != freeInstanceIndices.size()) {
        fmt::print("WARNING: Attempting to release buffers while some GameObjects still reference this");
        return false;
    }

    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        resourceManager.destroyBuffer(drawIndirectBuffers[i]);
        resourceManager.destroyBuffer(cullingAddressBuffers[i]);
        resourceManager.destroyBuffer(modelMatrixBuffers[i]);
        resourceManager.destroyBuffer(boundingSphereIndicesBuffers[i]);
    }

    freeInstanceIndices.clear();
    currentInstanceCount = 0;
    return true;
}

int32_t RenderObject::getFreeInstanceIndex()
{
    if (freeInstanceIndices.empty()) {
        const size_t oldSize = freeInstanceIndices.size();
        const size_t newSize = freeInstanceIndices.size() + 10;
        freeInstanceIndices.reserve(newSize);
        for (int32_t i = oldSize; i < newSize; ++i) { freeInstanceIndices.insert(i); }
        currentInstanceCount = freeInstanceIndices.size();
        dirty();
    }

    assert(!freeInstanceIndices.empty());
    const uint32_t index = *freeInstanceIndices.begin();
    freeInstanceIndices.erase(freeInstanceIndices.begin());
    return index;
}
}
