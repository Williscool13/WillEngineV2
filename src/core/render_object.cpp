//
// Created by William on 2024-08-24.

#include "render_object.h"

#include "../renderer/vk_descriptors.h"
#include "engine.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "glm/detail/type_quat.hpp"
#include "glm/gtx/matrix_decompose.hpp"

/**
 * Material and Instance Buffer Addresses
 */
VkDescriptorSetLayout RenderObject::addressesDescriptorSetLayout{VK_NULL_HANDLE};
/**
 * Sampler and Image Arrays
 */
VkDescriptorSetLayout RenderObject::textureDescriptorSetLayout{VK_NULL_HANDLE};
/**
 * Frustum Culling Data Buffer Addresses
 */
VkDescriptorSetLayout RenderObject::frustumCullingDescriptorSetLayout{VK_NULL_HANDLE};
int RenderObject::renderObjectCount{0};

RenderObject::RenderObject(Engine* engine, std::string_view gltfFilepath)
{
    creator = engine;

    if (renderObjectCount == 0) {
        // Descriptor Layouts
        {
            {
                // Render binding 0
                DescriptorLayoutBuilder layoutBuilder;
                layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
                addressesDescriptorSetLayout = layoutBuilder.build(creator->getDevice(),
                                                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
                                                                   , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
            } {
                // Render binding 1
                DescriptorLayoutBuilder layoutBuilder;
                layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SAMPLER_COUNT);
                layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_IMAGES_COUNT);

                textureDescriptorSetLayout = layoutBuilder.build(creator->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT
                                                                 , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
            } {
                DescriptorLayoutBuilder layoutBuilder;
                layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
                frustumCullingDescriptorSetLayout = layoutBuilder.build(creator->getDevice(), VK_SHADER_STAGE_COMPUTE_BIT
                                                                        , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
            }
        }
    }

    renderObjectCount++;

    parseModel(engine, gltfFilepath);

    const VkDeviceAddress materialBufferAddress = engine->getBufferAddress(materialBuffer);
    bufferAddresses = engine->createBuffer(sizeof(VkDeviceAddress) * 2
                                           , VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           VMA_MEMORY_USAGE_CPU_TO_GPU);
    memcpy(bufferAddresses.info.pMappedData, &materialBufferAddress, sizeof(VkDeviceAddress));
    addressesDescriptorBuffer = DescriptorBufferUniform(engine->getInstance(), engine->getDevice()
                                                        , engine->getPhysicalDevice(), engine->getAllocator(), addressesDescriptorSetLayout, 1);

    DescriptorUniformData addressesUniformData;
    addressesUniformData.allocSize = sizeof(VkDeviceAddress) * 2;
    addressesUniformData.uniformBuffer = bufferAddresses;
    std::vector uniforms = {addressesUniformData};
    addressesDescriptorBuffer.setupData(creator->getDevice(), uniforms);

    // Culling Bounds
    AllocatedBuffer meshBoundsStaging = creator->createStagingBuffer(sizeof(BoundingSphere) * boundingSpheres.size());
    memcpy(meshBoundsStaging.info.pMappedData, boundingSpheres.data(), sizeof(BoundingSphere) * boundingSpheres.size());
    meshBoundsBuffer = creator->createBuffer(sizeof(BoundingSphere) * boundingSpheres.size(),
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                             VMA_MEMORY_USAGE_GPU_ONLY);
    creator->copyBuffer(meshBoundsStaging, meshBoundsBuffer, sizeof(BoundingSphere) * boundingSpheres.size());
    creator->destroyBuffer(meshBoundsStaging);

    frustumCullingDescriptorBuffer = DescriptorBufferUniform(engine->getInstance(), engine->getDevice(), engine->getPhysicalDevice(),
                                                             engine->getAllocator(), frustumCullingDescriptorSetLayout, 1);
    frustumBufferAddresses = creator->createBuffer(sizeof(FrustumCullingBuffers),
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                                   , VMA_MEMORY_USAGE_GPU_ONLY);
    DescriptorUniformData cullingAddressesUniformData;
    cullingAddressesUniformData.allocSize = sizeof(FrustumCullingBuffers);
    cullingAddressesUniformData.uniformBuffer = frustumBufferAddresses;
    std::vector cullingUniforms = {cullingAddressesUniformData};
    frustumCullingDescriptorBuffer.setupData(creator->getDevice(), cullingUniforms);
}

RenderObject::~RenderObject()
{
    renderObjectCount--;
    if (renderObjectCount == 0) {
        vkDestroyDescriptorSetLayout(creator->getDevice(), addressesDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(creator->getDevice(), textureDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(creator->getDevice(), frustumCullingDescriptorSetLayout, nullptr);
    }

    creator->destroyBuffer(indexBuffer);
    creator->destroyBuffer(vertexBuffer);
    creator->destroyBuffer(drawIndirectBuffer);

    creator->destroyBuffer(bufferAddresses);

    creator->destroyBuffer(materialBuffer);
    creator->destroyBuffer(instanceBuffer);

    creator->destroyBuffer(meshBoundsBuffer);
    creator->destroyBuffer(meshIndicesBuffer);
    creator->destroyBuffer(frustumBufferAddresses);

    textureDescriptorBuffer.destroy(creator->getDevice(), creator->getAllocator());
    addressesDescriptorBuffer.destroy(creator->getDevice(), creator->getAllocator());
    frustumCullingDescriptorBuffer.destroy(creator->getDevice(), creator->getAllocator());

    for (auto& image : images) {
        if (image.image == Engine::errorCheckerboardImage.image
            || image.image == Engine::whiteImage.image) {
            //dont destroy the default images
            continue;
        }

        creator->destroyImage(image);
    }

    for (auto& sampler : samplers) {
        if (sampler == Engine::defaultSamplerNearest
            || sampler == Engine::defaultSamplerLinear) {
            //dont destroy the default samplers
            continue;
        }
        vkDestroySampler(creator->getDevice(), sampler, nullptr);
    }
}

void RenderObject::parseModel(Engine* engine, std::string_view gltfFilepath)
{
    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember
                                 | fastgltf::Options::AllowDouble
                                 | fastgltf::Options::LoadExternalBuffers
                                 | fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer data;
    data.FromPath(gltfFilepath);
    fastgltf::Asset gltf;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(gltfFilepath);
    if (!static_cast<bool>(gltfFile)) { fmt::print("Failed to open glTF file: {}\n", fastgltf::getErrorMessage(gltfFile.error())); }

    std::filesystem::path path = gltfFilepath;
    auto load = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
    if (!load) { fmt::print("Failed to load glTF: {}\n", fastgltf::to_underlying(load.error())); }

    gltf = std::move(load.get());


    constexpr uint32_t samplerOffset = 1; // default sampler at 0
    assert(Engine::defaultSamplerNearest != VK_NULL_HANDLE);
    samplers.push_back(Engine::defaultSamplerNearest);
    // Samplers
    {
        for (const fastgltf::Sampler& gltfSampler : gltf.samplers) {
            VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
            sampl.maxLod = VK_LOD_CLAMP_NONE;
            sampl.minLod = 0;

            sampl.magFilter = vk_helpers::extractFilter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
            sampl.minFilter = vk_helpers::extractFilter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));

            sampl.mipmapMode = vk_helpers::extractMipmapMode(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));

            VkSampler newSampler;
            vkCreateSampler(engine->getDevice(), &sampl, nullptr, &newSampler);

            samplers.push_back(newSampler);
        }
    }
    assert(samplers.size() == gltf.samplers.size() + samplerOffset);
    assert(samplers.size() <= MAX_SAMPLER_COUNT);

    constexpr uint32_t imageOffset = 1; // default texture at 0
    assert(Engine::whiteImage.image != VK_NULL_HANDLE);
    images.push_back(Engine::whiteImage);
    // Images
    {
        for (const fastgltf::Image& gltfImage : gltf.images) {
            std::optional<AllocatedImage> newImage = vk_helpers::loadImage(engine, gltf, gltfImage, path.parent_path());
            if (newImage.has_value()) {
                images.push_back(*newImage);
            } else {
                images.push_back(Engine::errorCheckerboardImage);
            }
        }
    }
    assert(images.size() == gltf.images.size() + imageOffset);
    assert(images.size() <= MAX_IMAGES_COUNT);

    // Binding Images/Samplers
    {
        std::vector<DescriptorImageData> textureDescriptors;
        // samplers
        {
            for (VkSampler sampler : samplers) {
                VkDescriptorImageInfo samplerInfo{.sampler = sampler};
                textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo, false});
            }

            size_t remaining = MAX_SAMPLER_COUNT - samplers.size();
            VkDescriptorImageInfo samplerInfo{};
            for (int i = 0; i < remaining; i++) {
                textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo, true});
            }
        }
        // images
        {
            for (AllocatedImage image : images) {
                VkDescriptorImageInfo imageInfo{.imageView = image.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo, false});
            }
        }

        // init descriptor buffer
        textureDescriptorBuffer = DescriptorBufferSampler(engine->getInstance(), engine->getDevice(), engine->getPhysicalDevice(),
                                                          engine->getAllocator(), textureDescriptorSetLayout, 1);
        textureDescriptorBuffer.setupData(engine->getDevice(), textureDescriptors);
    }

    // Materials
    std::vector<Material> materials;
    int32_t materialOffset = 1; // default material at 0
    materials.emplace_back();
    materials[0].textureImageIndices = glm::vec4(0, 0, 0, 0);
    materials[0].textureSamplerIndices = glm::vec4(0, 0, 0, 0);
    //  actual materials
    {
        for (const fastgltf::Material& gltfMaterial : gltf.materials) {
            Material material{};
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

            vk_helpers::loadTexture(gltfMaterial.pbrData.baseColorTexture, gltf, material.textureImageIndices.x,
                                    material.textureSamplerIndices.x, imageOffset, samplerOffset);
            vk_helpers::loadTexture(gltfMaterial.pbrData.metallicRoughnessTexture, gltf, material.textureImageIndices.y,
                                    material.textureSamplerIndices.y, imageOffset, samplerOffset);

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

            materials.push_back(material);
        }
    }
    assert(materials.size() == gltf.materials.size() + materialOffset);

    AllocatedBuffer materialStaging = creator->createStagingBuffer(materials.size() * sizeof(Material));
    memcpy(materialStaging.info.pMappedData, materials.data(), materials.size() * sizeof(Material));
    materialBuffer = engine->createBuffer(materials.size() * sizeof(Material)
                                          , VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                          , VMA_MEMORY_USAGE_GPU_ONLY);

    creator->copyBuffer(materialStaging, materialBuffer, materials.size() * sizeof(Material));
    creator->destroyBuffer(materialStaging);

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        std::vector<Vertex> meshVertices;
        std::vector<uint32_t> meshIndices;
        bool hasTransparentPrimitives = false;
        for (fastgltf::Primitive& p : mesh.primitives) {
            size_t initialVtx = meshVertices.size();
            glm::uint32_t primitiveMaterialIndex{0};

            if (p.materialIndex.has_value()) {
                primitiveMaterialIndex = p.materialIndex.value() + materialOffset;
                hasTransparentPrimitives |= (static_cast<MaterialType>(materials[primitiveMaterialIndex].alphaCutoff.y) == MaterialType::TRANSPARENT);
            }

            // load indexes
            {
                const fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                meshIndices.reserve(meshIndices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor, [&](std::uint32_t idx) {
                    meshIndices.push_back(idx + static_cast<uint32_t>(initialVtx));
                });
            }

            // load vertex positions
            {
                // position is a REQUIRED property in gltf. No need to check if it exists
                const fastgltf::Attribute* positionIt = p.findAttribute("POSITION");
                const fastgltf::Accessor& posAccessor = gltf.accessors[positionIt->accessorIndex];
                meshVertices.resize(meshVertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, posAccessor, [&](fastgltf::math::fvec3 v, size_t index) {
                    Vertex newvtx{};
                    newvtx.position = {v.x(), v.y(), v.z()};
                    newvtx.materialIndex = primitiveMaterialIndex;
                    meshVertices[initialVtx + index] = newvtx;
                });
            }

            // load vertex normals
            const fastgltf::Attribute* normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex],
                                                                          [&](fastgltf::math::fvec3 n, size_t index) {
                                                                              meshVertices[initialVtx + index].normal = {n.x(), n.y(), n.z()};
                                                                          });
            }

            // load UVs
            const fastgltf::Attribute* uvs = p.findAttribute("TEXCOORD_0");
            if (uvs != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uvs->accessorIndex],
                                                                          [&](fastgltf::math::fvec2 uv, size_t index) {
                                                                              meshVertices[initialVtx + index].uv = {uv.x(), uv.y()};
                                                                          });
            }

            // load vertex colors
            const fastgltf::Attribute* colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex],
                                                                          [&](fastgltf::math::fvec4 color, size_t index) {
                                                                              meshVertices[initialVtx + index].color = {
                                                                                  color.x(), color.y(), color.z(), color.w()
                                                                              };
                                                                          });
            }
        }

        meshes.push_back(
            {
                .firstIndex = static_cast<uint32_t>(indices.size()),
                .indexCount = static_cast<uint32_t>(meshIndices.size()),
                .vertexOffset = static_cast<int32_t>(vertices.size()),
                .hasTransparents = hasTransparentPrimitives
            });
        vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
        indices.insert(indices.end(), meshIndices.begin(), meshIndices.end());

        boundingSpheres.emplace_back(meshVertices);
    }
    // Upload the vertices and indices into their buffers
    {
        AllocatedBuffer vertexStaging = engine->createStagingBuffer(vertices.size() * sizeof(Vertex));
        memcpy(vertexStaging.info.pMappedData, vertices.data(), vertices.size() * sizeof(Vertex));

        AllocatedBuffer indexStaging = engine->createStagingBuffer(indices.size() * sizeof(uint32_t));
        memcpy(indexStaging.info.pMappedData, indices.data(), indices.size() * sizeof(uint32_t));


        vertexBuffer = engine->createBuffer(vertices.size() * sizeof(Vertex)
                                            , VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                            , VMA_MEMORY_USAGE_GPU_ONLY);

        indexBuffer = engine->createBuffer(indices.size() * sizeof(uint32_t)
                                           , VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                           , VMA_MEMORY_USAGE_GPU_ONLY);


        engine->copyBuffer(vertexStaging, vertexBuffer, vertices.size() * sizeof(Vertex));
        engine->copyBuffer(indexStaging, indexBuffer, indices.size() * sizeof(uint32_t));

        engine->destroyBuffer(vertexStaging);
        engine->destroyBuffer(indexStaging);
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

                    glm::vec3 translation = glm::vec3(glmMatrix[3]);
                    glm::vec3 scale = glm::vec3(
                        glm::length(glm::vec3(glmMatrix[0])),
                        glm::length(glm::vec3(glmMatrix[1])),
                        glm::length(glm::vec3(glmMatrix[2]))
                    );
                    glm::quat rotation = glm::quat_cast(glmMatrix);

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
            renderNodes[i].children.push_back(renderNodes[child]);
            renderNodes[child].parent = &renderNodes[i];
        }
    }


    for (int i = 0; i < renderNodes.size(); i++) {
        if (renderNodes[i].parent == nullptr) {
            topNodes.push_back(i);
        }

        if (renderNodes[i].meshIndex != -1) {
            instanceCount++;
        }
    }

    fmt::print("GLTF: {} | Samplers: {} | Images: {} | Materials: {} | Meshes: {} | Instances: {}\n", gltfFilepath, samplers.size() - samplerOffset,
               images.size() - imageOffset, materials.size() - materialOffset, meshes.size(), instanceCount);
}

GameObject* RenderObject::generateGameObject()
{
    expandInstanceBuffer(instanceCount);

    auto* superRoot = new GameObject();
    for (const int32_t rootNode : topNodes) {
        recursiveGenerateGameObject(renderNodes[rootNode], superRoot);
    }

    superRoot->refreshTransforms();

    uploadIndirectBuffer();
    updateComputeCullingBuffer();
    return superRoot;
}

void RenderObject::updateInstanceData(const InstanceData& value, const int32_t index) const
{
    if (instanceBuffer.buffer == VK_NULL_HANDLE) {
        return;
    }
    // TODO: Null pointer during destruction here
    auto basePtr = static_cast<char*>(instanceBuffer.info.pMappedData);
    void* target = basePtr + index * sizeof(InstanceData);
    memcpy(target, &value, sizeof(InstanceData));
}

void RenderObject::recursiveGenerateGameObject(const RenderNode& renderNode, GameObject* parent)
{
    const auto gameObject = new GameObject();

    if (renderNode.meshIndex != -1) {
        size_t instanceIndex = drawIndirectCommands.size();

        VkDrawIndexedIndirectCommand drawCommand;
        drawCommand.firstIndex = meshes[renderNode.meshIndex].firstIndex;
        drawCommand.indexCount = meshes[renderNode.meshIndex].indexCount;
        drawCommand.vertexOffset = meshes[renderNode.meshIndex].vertexOffset;

        drawCommand.instanceCount = 1;
        drawCommand.firstInstance = instanceIndex;

        gameObject->setRenderObjectReference(this, static_cast<int32_t>(instanceIndex));
        drawIndirectCommands.push_back(drawCommand);
        meshIndices.push_back(renderNode.meshIndex);
        assert(drawIndirectCommands.size() == meshIndices.size());
    }

    gameObject->transform.setTransform(renderNode.transform);
    gameObject->refreshTransforms();
    parent->addChild(gameObject, false);

    for (const auto& child : renderNode.children) {
        recursiveGenerateGameObject(child, gameObject);
    }
}

void RenderObject::expandInstanceBuffer(const uint32_t count, const bool copy)
{
    const uint32_t oldBufferSize = instanceBufferSize;
    instanceBufferSize += count;
    // CPU_TO_GPU because it can be modified any time by gameobjects
    const AllocatedBuffer tempInstanceBuffer = creator->createBuffer(instanceBufferSize * sizeof(InstanceData)
                                                                     , VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                                                     , VMA_MEMORY_USAGE_CPU_TO_GPU);

    if (copy && oldBufferSize > 0) {
        creator->copyBuffer(instanceBuffer, tempInstanceBuffer, oldBufferSize * sizeof(InstanceData));
        creator->destroyBuffer(instanceBuffer);
    }

    instanceBuffer = tempInstanceBuffer;

    const VkDeviceAddress instanceBufferAddress = creator->getBufferAddress(instanceBuffer);
    memcpy(static_cast<char*>(bufferAddresses.info.pMappedData) + sizeof(VkDeviceAddress), &instanceBufferAddress, sizeof(VkDeviceAddress));
}

void RenderObject::uploadIndirectBuffer()
{
    if (drawIndirectBuffer.buffer != VK_NULL_HANDLE) {
        creator->destroyBuffer(drawIndirectBuffer);
    }

    if (drawIndirectCommands.empty()) {
        return;
    }

    AllocatedBuffer indirectStaging = creator->createStagingBuffer(drawIndirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
    memcpy(indirectStaging.info.pMappedData, drawIndirectCommands.data(), drawIndirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
    drawIndirectBuffer = creator->createBuffer(drawIndirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand)
                                               , VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                               , VMA_MEMORY_USAGE_GPU_ONLY);

    creator->copyBuffer(indirectStaging, drawIndirectBuffer, drawIndirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
    creator->destroyBuffer(indirectStaging);
}

void RenderObject::updateComputeCullingBuffer()
{
    if (drawIndirectBuffer.buffer == VK_NULL_HANDLE || instanceBuffer.buffer == VK_NULL_HANDLE || meshBoundsBuffer.buffer == VK_NULL_HANDLE) {
        return;
    }

    // both should be equal in all cases
    if (drawIndirectCommands.empty() || meshIndices.empty()) {
        return;
    }

    if (meshIndicesBuffer.buffer != VK_NULL_HANDLE) {
        creator->destroyBuffer(meshIndicesBuffer);
    }

    AllocatedBuffer meshIndicesStaging = creator->createStagingBuffer(meshIndices.size() * sizeof(uint32_t));
    memcpy(meshIndicesStaging.info.pMappedData, meshIndices.data(), meshIndices.size() * sizeof(uint32_t));
    meshIndicesBuffer = creator->createBuffer(meshIndices.size() * sizeof(uint32_t)
                                              , VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                              , VMA_MEMORY_USAGE_GPU_ONLY);

    creator->copyBuffer(meshIndicesStaging, meshIndicesBuffer, meshIndices.size() * sizeof(uint32_t));
    creator->destroyBuffer(meshIndicesStaging);


    FrustumCullingBuffers cullingBuffers{};
    cullingBuffers.commandBuffer = creator->getBufferAddress(drawIndirectBuffer);
    cullingBuffers.commandBufferCount = drawIndirectCommands.size();
    cullingBuffers.modelMatrixBuffer = creator->getBufferAddress(instanceBuffer);
    cullingBuffers.meshBoundsBuffer = creator->getBufferAddress(meshBoundsBuffer);

    AllocatedBuffer frustumCullingStaging = creator->createStagingBuffer(sizeof(FrustumCullingBuffers));
    memcpy(frustumCullingStaging.info.pMappedData, &cullingBuffers, sizeof(FrustumCullingBuffers));
    creator->copyBuffer(frustumCullingStaging, frustumBufferAddresses, sizeof(FrustumCullingBuffers));
    creator->destroyBuffer(frustumCullingStaging);
}
