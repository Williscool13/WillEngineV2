//
// Created by William on 2025-01-24.
//

#include "render_object.h"

#include <extern/fastgltf/include/fastgltf/core.hpp>
#include <extern/fastgltf/include/fastgltf/types.hpp>
#include <extern/fastgltf/include/fastgltf/tools.hpp>
#include <extern/fmt/include/fmt/format.h>
#include <vulkan/vulkan_core.h>

#include "src/renderer/vulkan_context.h"
#include "render_object_constants.h"
#include "src/core/game_object/game_object.h"
#include "src/util/file.h"
#include "src/util/model_utils.h"

namespace will_engine
{
RenderObject::RenderObject(ResourceManager& resourceManager, const std::filesystem::path& willmodelPath, const std::filesystem::path& gltfFilepath,
                           std::string name, const uint32_t renderObjectId)
    : willmodelPath(willmodelPath), gltfPath(gltfFilepath), name(std::move(name)), renderObjectId(renderObjectId), resourceManager(resourceManager)
{}

RenderObject::~RenderObject()
{
    // todo: deferred buffer destruction for multi-buffer
    unload();
}

void RenderObject::update(const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    if (!bIsLoaded) { return; }
    updateBuffers(currentFrameOverlap, previousFrameOverlap);

    for (const std::pair renderablePair : renderableMap) {
        IRenderable* renderable = renderablePair.first;
        if (renderable->getRenderFramesToUpdate() > 0) {
            const int32_t instanceIndex = renderablePair.second.instanceIndex;

            if (instanceIndex < 0 || instanceIndex >= currentInstanceCount) {
                fmt::print("Instance from renderable is not a valid index");
                continue;
            }

            const AllocatedBuffer& previousFrameModelMatrix = modelMatrixBuffers[previousFrameOverlap];
            const AllocatedBuffer& currentFrameModelMatrix = modelMatrixBuffers[currentFrameOverlap];

            auto prevModel = reinterpret_cast<InstanceData*>(static_cast<char*>(previousFrameModelMatrix.info.pMappedData) + instanceIndex * sizeof(
                                                                 InstanceData));
            auto currentModel = reinterpret_cast<InstanceData*>(
                static_cast<char*>(currentFrameModelMatrix.info.pMappedData) + instanceIndex * sizeof(InstanceData));

            assert(reinterpret_cast<uintptr_t>(prevModel) % alignof(InstanceData) == 0 && "Misaligned instance data access");
            assert(reinterpret_cast<uintptr_t>(currentModel) % alignof(InstanceData) == 0 && "Misaligned instance data access");

            currentModel->previousModelMatrix = prevModel->currentModelMatrix;
            currentModel->currentModelMatrix = renderable->getModelMatrix();
            currentModel->flags[0] = renderable->isVisible();
            currentModel->flags[1] = renderable->isShadowCaster();

            renderable->setRenderFramesToUpdate(renderable->getRenderFramesToUpdate() - 1);
        }
    }
}

bool RenderObject::updateBuffers(const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    if (bufferFramesToUpdate <= 0) { return true; }

    AllocatedBuffer& currentPrimitiveBuffer = primitiveDataBuffers[currentFrameOverlap];
    const AllocatedBuffer& previousPrimitiveBuffer = primitiveDataBuffers[previousFrameOverlap];

    // Recreate the primitive buffer if needed
    size_t latestPrimitiveBufferSize = currentPrimitiveCount * sizeof(PrimitiveData);
    if (currentPrimitiveBuffer.info.size != latestPrimitiveBufferSize) {
        const AllocatedBuffer newPrimitiveBuffer = resourceManager.createHostRandomBuffer(latestPrimitiveBufferSize,
                                                                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        // copy data from previous frame, as it is the most up to date
        const size_t sizeToCopy = glm::min(latestPrimitiveBufferSize, previousPrimitiveBuffer.info.size);
        if (sizeToCopy > 0) {
            resourceManager.copyBuffer(previousPrimitiveBuffer, newPrimitiveBuffer, sizeToCopy);
        }
        resourceManager.destroyBuffer(currentPrimitiveBuffer);
        currentPrimitiveBuffer = newPrimitiveBuffer;

        const VkDeviceAddress primitiveBufferAddress = resourceManager.getBufferAddress(currentPrimitiveBuffer);
        memcpy(static_cast<char*>(addressBuffers[currentFrameOverlap].info.pMappedData) + sizeof(VkDeviceAddress), &primitiveBufferAddress,
               sizeof(VkDeviceAddress));
    }

    // Upload primitive data
    {
        for (auto& pair : primitiveDataMap) {
            auto mappedData = static_cast<char*>(currentPrimitiveBuffer.info.pMappedData);
            auto pPrimitiveData = reinterpret_cast<PrimitiveData*>(mappedData + sizeof(PrimitiveData) * pair.first);

            *pPrimitiveData = pair.second;
        }
        // vk_helpers::synchronizeUniform(cmd, currentPrimitiveBuffer, VK_PIPELINE_STAGE_2_HOST_BIT
        //                                , VK_ACCESS_2_HOST_WRITE_BIT
        //                                , VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
        //                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
        //                                , VK_ACCESS_2_UNIFORM_READ_BIT);
    }

    AllocatedBuffer& currentInstanceBuffer = modelMatrixBuffers[currentFrameOverlap];
    const AllocatedBuffer& previousInstanceBuffer = modelMatrixBuffers[previousFrameOverlap];

    // sizes don't match, need to recreate the buffer
    if (currentInstanceBuffer.info.size != currentInstanceCount * sizeof(InstanceData)) {
        const AllocatedBuffer newInstanceBuffer = resourceManager.createHostRandomBuffer(currentInstanceCount * sizeof(InstanceData),
                                                                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        // copy data from previous frame, as it is the most up to date
        const size_t sizeToCopy = glm::min(currentInstanceCount * sizeof(InstanceData), previousInstanceBuffer.info.size);
        if (sizeToCopy > 0) {
            resourceManager.copyBuffer(previousInstanceBuffer, newInstanceBuffer, sizeToCopy);
        }
        resourceManager.destroyBuffer(currentInstanceBuffer);
        currentInstanceBuffer = newInstanceBuffer;

        const VkDeviceAddress instanceBufferAddress = resourceManager.getBufferAddress(currentInstanceBuffer);
        memcpy(static_cast<char*>(addressBuffers[currentFrameOverlap].info.pMappedData) + sizeof(VkDeviceAddress) * 2, &instanceBufferAddress,
               sizeof(VkDeviceAddress));
    }

    // instance records could be stale,  need to ensure that it cleared/invalidated on GPU
    for (const uint32_t freeIndex : freeInstanceIndices) {
        auto targetModel = reinterpret_cast<InstanceData*>(static_cast<char*>(currentInstanceBuffer.info.pMappedData) + freeIndex * sizeof(
                                                               InstanceData));

        assert(reinterpret_cast<uintptr_t>(targetModel) % alignof(InstanceData) == 0 && "Misaligned instance data access");

        targetModel->previousModelMatrix = glm::identity<glm::mat4>();
        targetModel->currentModelMatrix = glm::identity<glm::mat4>();
        targetModel->flags = glm::vec4(0.0f);
    }

    AllocatedBuffer& currentOpaqueDrawIndirectBuffer = opaqueDrawIndirectBuffers[currentFrameOverlap];
    if (opaqueDrawCommands.empty()) {
        resourceManager.destroyBuffer(currentOpaqueDrawIndirectBuffer);
    }
    else {
        resourceManager.destroyBuffer(currentOpaqueDrawIndirectBuffer);
        AllocatedBuffer indirectStaging = resourceManager.createStagingBuffer(opaqueDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        memcpy(indirectStaging.info.pMappedData, opaqueDrawCommands.data(), opaqueDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        currentOpaqueDrawIndirectBuffer = resourceManager.createDeviceBuffer(opaqueDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand),
                                                                             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

        resourceManager.copyBuffer(indirectStaging, currentOpaqueDrawIndirectBuffer,
                                   opaqueDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        resourceManager.destroyBuffer(indirectStaging);

        const FrustumCullingBuffers cullingAddresses{
            .meshBoundsBuffer = resourceManager.getBufferAddress(meshBoundsBuffer),
            .commandBuffer = resourceManager.getBufferAddress(currentOpaqueDrawIndirectBuffer),
            .commandBufferCount = static_cast<uint32_t>(opaqueDrawCommands.size()),
            .padding = {},
        };

        const auto currentCullingAddressBuffers = opaqueCullingAddressBuffers[currentFrameOverlap];
        AllocatedBuffer stagingCullingAddressesBuffer = resourceManager.createStagingBuffer(sizeof(FrustumCullingBuffers));
        memcpy(stagingCullingAddressesBuffer.info.pMappedData, &cullingAddresses, sizeof(FrustumCullingBuffers));
        resourceManager.copyBuffer(stagingCullingAddressesBuffer, currentCullingAddressBuffers, sizeof(FrustumCullingBuffers));
        resourceManager.destroyBuffer(stagingCullingAddressesBuffer);
    }

    AllocatedBuffer& currentTransparentDrawIndirectBuffer = transparentDrawIndirectBuffers[currentFrameOverlap];
    if (transparentDrawCommands.empty()) {
        resourceManager.destroyBuffer(currentTransparentDrawIndirectBuffer);
    }
    else {
        resourceManager.destroyBuffer(currentTransparentDrawIndirectBuffer);
        AllocatedBuffer indirectStaging = resourceManager.createStagingBuffer(transparentDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        memcpy(indirectStaging.info.pMappedData, transparentDrawCommands.data(),
               transparentDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        currentTransparentDrawIndirectBuffer = resourceManager.createDeviceBuffer(
            transparentDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

        resourceManager.copyBuffer(indirectStaging, currentTransparentDrawIndirectBuffer,
                                   transparentDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        resourceManager.destroyBuffer(indirectStaging);

        const FrustumCullingBuffers cullingAddresses{
            .meshBoundsBuffer = resourceManager.getBufferAddress(meshBoundsBuffer),
            .commandBuffer = resourceManager.getBufferAddress(currentTransparentDrawIndirectBuffer),
            .commandBufferCount = static_cast<uint32_t>(transparentDrawCommands.size()),
            .padding = {},
        };

        const auto currentCullingAddressBuffers = transparentCullingAddressBuffers[currentFrameOverlap];
        AllocatedBuffer stagingCullingAddressesBuffer = resourceManager.createStagingBuffer(sizeof(FrustumCullingBuffers));
        memcpy(stagingCullingAddressesBuffer.info.pMappedData, &cullingAddresses, sizeof(FrustumCullingBuffers));
        resourceManager.copyBuffer(stagingCullingAddressesBuffer, currentCullingAddressBuffers, sizeof(FrustumCullingBuffers));
        resourceManager.destroyBuffer(stagingCullingAddressesBuffer);
    }

    bufferFramesToUpdate--;
    return false;
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

    dirty();
    return superRoot;
}

void RenderObject::recursiveGenerateGameObject(const RenderNode& renderNode, GameObject* parent)
{
    // todo: preferably RenderObject does not directly reference GameObject. Try using a static factory
    const auto gameObject = new GameObject();

    if (renderNode.meshIndex != -1) {
        IComponentContainer* _container = gameObject;
        auto newMeshComponent = components::ComponentFactory::getInstance().createComponent(
            components::MeshRendererComponent::getStaticType(), renderNode.name);
        _container->addComponent(std::move(newMeshComponent));
        if (components::MeshRendererComponent* meshRenderer = _container->getMeshRenderer()) {
            generateMesh(meshRenderer, renderNode.meshIndex);
        }
    }

    gameObject->setLocalTransform(renderNode.transform);
    parent->addChild(gameObject);

    for (const auto& child : renderNode.children) {
        recursiveGenerateGameObject(*child, gameObject);
    }
}

bool RenderObject::generateMesh(IRenderable* renderable, const int32_t meshIndex)
{
    if (renderable == nullptr) { return false; }
    if (meshIndex < 0 || meshIndex >= meshes.size()) { return false; }

    const uint32_t instanceIndex = getFreeInstanceIndex();

    const std::vector<Primitive>& meshPrimitives = meshes[meshIndex].primitives;
    opaqueDrawCommands.reserve(opaqueDrawCommands.size() + meshPrimitives.size());

    for (const Primitive primitive : meshPrimitives) {
        const uint32_t primitiveIndex = getFreePrimitiveIndex();
        opaqueDrawCommands.emplace_back();
        VkDrawIndexedIndirectCommand& indirectData = opaqueDrawCommands.back();
        indirectData.firstIndex = primitive.firstIndex;
        indirectData.indexCount = primitive.indexCount;
        indirectData.vertexOffset = primitive.vertexOffset;
        indirectData.instanceCount = 1;
        indirectData.firstInstance = primitiveIndex;

        if (primitive.bHasTransparent) {
            transparentDrawCommands.emplace_back();
            VkDrawIndexedIndirectCommand& transparentIndirectData = transparentDrawCommands.back();
            transparentIndirectData.firstIndex = primitive.firstIndex;
            transparentIndirectData.firstIndex = primitive.firstIndex;
            transparentIndirectData.indexCount = primitive.indexCount;
            transparentIndirectData.vertexOffset = primitive.vertexOffset;
            transparentIndirectData.instanceCount = 1;
            transparentIndirectData.firstInstance = primitiveIndex;
        }

        primitiveDataMap[primitiveIndex] = {
            primitive.materialIndex, instanceIndex, primitive.boundingSphereIndex, primitive.bHasTransparent ? 1u : 0u
        };
    }

    renderable->setRenderObjectReference(this, meshIndex);
    renderable->dirty();

    RenderableProperties renderableProperties{
        instanceIndex
    };
    renderableMap.insert({renderable, renderableProperties});

    dirty();
    return true;
}

bool RenderObject::releaseInstanceIndex(IRenderable* renderable)
{
    const auto it = renderableMap.find(renderable);
    if (it == renderableMap.end()) {
        fmt::print("WARNING: Render object instructed to release instance index when it is already free (not found in map)");
        return false;
    }

    for (auto pair : primitiveDataMap) {
        if (pair.second.instanceDataIndex == it->second.instanceIndex) {
            if (freePrimitiveIndices.contains(pair.first)) {
                fmt::print(
                    "WARNING: Render object instructed to release primitive index when it is already free (free indices already contains the index)");
                continue;
            }

            freePrimitiveIndices.insert(pair.first);

            for (size_t i = opaqueDrawCommands.size(); i > 0; --i) {
                const size_t index = i - 1;
                if (opaqueDrawCommands[index].firstInstance == pair.first) {
                    opaqueDrawCommands.erase(opaqueDrawCommands.begin() + index);
                }
            }

            for (size_t i = transparentDrawCommands.size(); i > 0; --i) {
                const size_t index = i - 1;
                if (transparentDrawCommands[index].firstInstance == pair.first) {
                    transparentDrawCommands.erase(transparentDrawCommands.begin() + index);
                }
            }
        }
    }

    for (auto freeIndex : freePrimitiveIndices) {
        primitiveDataMap.erase(freeIndex);
    }

    renderableMap.erase(renderable);
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
    if (!static_cast<bool>(gltfFile)) {
        fmt::print("Failed to open glTF file ({}): {}\n", gltfFilepath.filename().string(), getErrorMessage(gltfFile.error()));
    }

    auto load = parser.loadGltf(gltfFile.get(), gltfFilepath.parent_path(), gltfOptions);
    if (!load) {
        fmt::print("Failed to load glTF: {}\n", to_underlying(load.error()));
        return false;
    }

    fastgltf::Asset gltf = std::move(load.get());

    samplers.reserve(gltf.samplers.size() + model_utils::samplerOffset);
    samplers.push_back(this->resourceManager.getDefaultSamplerNearest());
    for (const fastgltf::Sampler& gltfSampler : gltf.samplers) {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0;

        samplerInfo.magFilter = model_utils::extractFilter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = model_utils::extractFilter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));


        samplerInfo.mipmapMode = model_utils::extractMipMapMode(gltfSampler.minFilter.value_or(fastgltf::Filter::Linear));
        samplers.emplace_back(this->resourceManager.createSampler(samplerInfo));
    }

    assert(samplers.size() <= render_object_constants::MAX_SAMPLER_COUNT);

    images.reserve(gltf.images.size() + model_utils::imageOffset);
    images.push_back(this->resourceManager.getWhiteImage());
    for (const fastgltf::Image& gltfImage : gltf.images) {
        std::optional<AllocatedImage> newImage = model_utils::loadImage(resourceManager, gltf, gltfImage, gltfFilepath.parent_path());
        if (newImage.has_value()) {
            images.push_back(*newImage);
        }
        else {
            images.push_back(this->resourceManager.getErrorCheckerboardImage());
        }
    }

    assert(images.size() <= render_object_constants::MAX_IMAGES_COUNT);

    uint32_t materialOffset = 1;
    // default material at 0
    materials.reserve(gltf.materials.size() + materialOffset);
    MaterialProperties defaultMaterial{
        glm::vec4(1.0f),
        {0.0f, 1.0f, 0.0f, 0.0f},
        glm::vec4(0.0f),
        glm::vec4(0.0f),
        {1.0f, 0.0f, 0.0f, 0.0f}
    };
    materials.push_back(defaultMaterial);
    for (const fastgltf::Material& gltfMaterial : gltf.materials) {
        MaterialProperties material = model_utils::extractMaterial(gltf, gltfMaterial);
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

            std::vector<VertexPosition> primitiveVertexPositions{};
            std::vector<VertexProperty> primitiveVertexProperties{};
            std::vector<uint32_t> primitiveIndices{};

            if (p.materialIndex.has_value()) {
                primitiveData.materialIndex = p.materialIndex.value() + materialOffset;
                primitiveData.bHasTransparent = (static_cast<MaterialType>(materials[primitiveData.materialIndex].alphaCutoff.y) ==
                                                 MaterialType::TRANSPARENT);
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
            primitiveVertexPositions.resize(posAccessor.count);
            primitiveVertexProperties.resize(posAccessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, posAccessor, [&](fastgltf::math::fvec3 v, const size_t index) {
                VertexPosition newVertexPos{};
                newVertexPos.position = {v.x(), v.y(), v.z()};
                primitiveVertexPositions[index] = newVertexPos;

                const VertexProperty newVertexProp{};
                primitiveVertexProperties[index] = newVertexProp;
            });


            // NORMALS
            const fastgltf::Attribute* normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex],
                                                                          [&](fastgltf::math::fvec3 n, const size_t index) {
                                                                              primitiveVertexProperties[index].normal = {n.x(), n.y(), n.z()};
                                                                          });
            }

            // UV
            const fastgltf::Attribute* uvs = p.findAttribute("TEXCOORD_0");
            if (uvs != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uvs->accessorIndex],
                                                                          [&](fastgltf::math::fvec2 uv, const size_t index) {
                                                                              primitiveVertexProperties[index].uv = {uv.x(), uv.y()};
                                                                          });
            }

            // VERTEX COLOR
            const fastgltf::Attribute* colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex],
                                                                          [&](fastgltf::math::fvec4 color, const size_t index) {
                                                                              primitiveVertexProperties[index].color = {
                                                                                  color.x(), color.y(), color.z(), color.w()
                                                                              };
                                                                          });
            }

            boundingSpheres.emplace_back(primitiveVertexPositions);

            primitiveData.firstIndex = static_cast<uint32_t>(indices.size());
            primitiveData.vertexOffset = static_cast<int32_t>(vertexPositions.size());
            primitiveData.indexCount = static_cast<uint32_t>(primitiveIndices.size());
            primitiveData.boundingSphereIndex = static_cast<uint32_t>(boundingSpheres.size() - 1);

            vertexPositions.insert(vertexPositions.end(), primitiveVertexPositions.begin(), primitiveVertexPositions.end());
            vertexProperties.insert(vertexProperties.end(), primitiveVertexProperties.begin(), primitiveVertexProperties.end());
            indices.insert(indices.end(), primitiveIndices.begin(), primitiveIndices.end());
            meshData.primitives.push_back(primitiveData);
        }

        meshes.push_back(meshData);
    }

    for (const fastgltf::Node& node : gltf.nodes) {
        RenderNode renderNode{};
        renderNode.name = node.name;

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
    fmt::print("GLTF: {} | Sampl: {} | Imag: {} | Mats: {} | Mesh: {} | Prim: {} | Inst: {} | in {}ms\n",
               file::getFileName(gltfFilepath.filename().string().c_str()),
               samplers.size() - model_utils::samplerOffset,
               images.size() - model_utils::imageOffset, materials.size() - materialOffset, meshes.size(), primitiveCount, instanceCount, time);
    return true;
}

void RenderObject::load()
{
    if (bIsLoaded) {
        fmt::print("Render Object attempted to load when it is already loaded");
        return;
    }

    freeInstanceIndices.reserve(10);
    for (int32_t i = 0; i < 10; ++i) { freeInstanceIndices.insert(i); }
    currentInstanceCount = freeInstanceIndices.size();

    freePrimitiveIndices.reserve(10);
    for (int32_t i = 0; i < 10; ++i) { freePrimitiveIndices.insert(i); }
    currentPrimitiveCount = freePrimitiveIndices.size();

    if (!parseGltf(gltfPath)) { return; }

    std::vector<DescriptorImageData> textureDescriptors;
    for (const VkSampler sampler : samplers) {
        textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, {.sampler = sampler}, false});
    }

    const size_t remaining = render_object_constants::MAX_SAMPLER_COUNT - samplers.size();
    for (int i = 0; i < remaining; i++) {
        textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, {}, true});
    }

    for (const AllocatedImage& image : images) {
        textureDescriptors.push_back({
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, {.imageView = image.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, false
        });
    }


    textureDescriptorBuffer = resourceManager.createDescriptorBufferSampler(resourceManager.getTexturesLayout(), 1);
    resourceManager.setupDescriptorBufferSampler(textureDescriptorBuffer, textureDescriptors, 0);


    AllocatedBuffer materialStaging = resourceManager.createStagingBuffer(materials.size() * sizeof(MaterialProperties));
    memcpy(materialStaging.info.pMappedData, materials.data(), materials.size() * sizeof(MaterialProperties));
    materialBuffer = resourceManager.createDeviceBuffer(materials.size() * sizeof(MaterialProperties));
    resourceManager.copyBuffer(materialStaging, materialBuffer, materials.size() * sizeof(MaterialProperties));
    resourceManager.destroyBuffer(materialStaging);

    size_t vertexCount = vertexPositions.size();
    uint64_t vertexPositionSize = vertexCount * sizeof(VertexPosition);

    AllocatedBuffer vertexPositionStaging = resourceManager.createStagingBuffer(vertexPositionSize);
    memcpy(vertexPositionStaging.info.pMappedData, vertexPositions.data(), vertexPositionSize);
    vertexPositionBuffer = resourceManager.createDeviceBuffer(vertexPositionSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    resourceManager.copyBuffer(vertexPositionStaging, vertexPositionBuffer, vertexPositionSize);
    resourceManager.destroyBuffer(vertexPositionStaging);

    uint64_t vertexPropertiesSize = vertexCount * sizeof(VertexProperty);
    AllocatedBuffer vertexPropertiesStaging = resourceManager.createStagingBuffer(vertexPropertiesSize);
    memcpy(vertexPropertiesStaging.info.pMappedData, vertexProperties.data(), vertexPropertiesSize);
    vertexPropertyBuffer = resourceManager.createDeviceBuffer(vertexPropertiesSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    resourceManager.copyBuffer(vertexPropertiesStaging, vertexPropertyBuffer, vertexPropertiesSize);
    resourceManager.destroyBuffer(vertexPropertiesStaging);

    AllocatedBuffer indexStaging = resourceManager.createStagingBuffer(indices.size() * sizeof(uint32_t));
    memcpy(indexStaging.info.pMappedData, indices.data(), indices.size() * sizeof(uint32_t));
    indexBuffer = resourceManager.createDeviceBuffer(indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    resourceManager.copyBuffer(indexStaging, indexBuffer, indices.size() * sizeof(uint32_t));
    resourceManager.destroyBuffer(indexStaging);

    // Addresses (Texture and Uniform model data)
    // todo: make address buffer for statics (dont need multiple)
    addressesDescriptorBuffer = resourceManager.createDescriptorBufferUniform(resourceManager.getRenderObjectAddressesLayout(), FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; i++) {
        constexpr size_t addressesSize = sizeof(VkDeviceAddress) * 3;
        addressBuffers[i] = resourceManager.createHostSequentialBuffer(addressesSize);
        DescriptorUniformData addressesUniformData{
            .uniformBuffer = addressBuffers[i],
            .allocSize = addressesSize,
        };
        resourceManager.setupDescriptorBufferUniform(addressesDescriptorBuffer, {addressesUniformData}, i);

        modelMatrixBuffers[i] = resourceManager.createHostRandomBuffer(currentInstanceCount * sizeof(InstanceData),
                                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        primitiveDataBuffers[i] = resourceManager.createHostRandomBuffer(currentPrimitiveCount * sizeof(PrimitiveData),
                                                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        const VkDeviceAddress materialBufferAddress = resourceManager.getBufferAddress(materialBuffer);
        const VkDeviceAddress primitiveDataBufferAddress = resourceManager.getBufferAddress(primitiveDataBuffers[i]);
        const VkDeviceAddress instanceBufferAddress = resourceManager.getBufferAddress(modelMatrixBuffers[i]);
        const VkDeviceAddress addresses[3] = {materialBufferAddress, primitiveDataBufferAddress, instanceBufferAddress};
        memcpy(addressBuffers[i].info.pMappedData, addresses, addressesSize);
    }

    frustumCullingDescriptorBuffer = resourceManager.createDescriptorBufferUniform(resourceManager.getFrustumCullLayout(), FRAME_OVERLAP * 2);
    // In 2 Frame Overlap: 0 and 1 for Opaque, 2 and 3 for Transparent
    // Opaque Culling Buffer
    for (int32_t i = 0; i < FRAME_OVERLAP; i++) {
        opaqueCullingAddressBuffers[i] = resourceManager.createDeviceBuffer(sizeof(FrustumCullingBuffers));
        const DescriptorUniformData cullingAddressesUniformData{
            .uniformBuffer = opaqueCullingAddressBuffers[i],
            .allocSize = sizeof(FrustumCullingBuffers),
        };
        resourceManager.setupDescriptorBufferUniform(frustumCullingDescriptorBuffer, {cullingAddressesUniformData}, i);
    }
    // Transparent Culling Buffer
    for (int32_t i = 0; i < FRAME_OVERLAP; i++) {
        transparentCullingAddressBuffers[i] = resourceManager.createDeviceBuffer(sizeof(FrustumCullingBuffers));
        const DescriptorUniformData cullingAddressesUniformData{
            .uniformBuffer = transparentCullingAddressBuffers[i],
            .allocSize = sizeof(FrustumCullingBuffers),
        };

        // Descriptor Buffer Offset
        int32_t index = i + FRAME_OVERLAP;
        resourceManager.setupDescriptorBufferUniform(frustumCullingDescriptorBuffer, {cullingAddressesUniformData}, index);
    }

    AllocatedBuffer meshBoundsStaging = resourceManager.createStagingBuffer(sizeof(BoundingSphere) * boundingSpheres.size());
    memcpy(meshBoundsStaging.info.pMappedData, boundingSpheres.data(), sizeof(BoundingSphere) * boundingSpheres.size());
    meshBoundsBuffer = resourceManager.createDeviceBuffer(sizeof(BoundingSphere) * boundingSpheres.size());
    resourceManager.copyBuffer(meshBoundsStaging, meshBoundsBuffer, sizeof(BoundingSphere) * boundingSpheres.size());
    resourceManager.destroyBuffer(meshBoundsStaging);


    bIsLoaded = true;
}

void RenderObject::unload()
{
    std::vector<IRenderable*> renderables{};
    renderables.reserve(renderableMap.size());
    for (std::pair renderable : renderableMap) {
        renderables.push_back(renderable.first);
    }

    for (const auto renderable : renderables) {
        renderable->releaseMesh();
    }

    renderables.clear();
    renderableMap.clear();

    for (auto& image : images) {
        if (image.image == resourceManager.getErrorCheckerboardImage().image || image.image == resourceManager.getWhiteImage().image) {
            //dont destroy the default images
            continue;
        }

        resourceManager.destroyImage(image);
    }

    for (auto& sampler : samplers) {
        if (sampler == resourceManager.getDefaultSamplerNearest() || sampler == resourceManager.getDefaultSamplerLinear()) {
            //dont destroy the default samplers
            continue;
        }

        resourceManager.destroySampler(sampler);
    }

    resourceManager.destroyBuffer(vertexPositionBuffer);
    resourceManager.destroyBuffer(vertexPropertyBuffer);
    resourceManager.destroyBuffer(indexBuffer);

    resourceManager.destroyBuffer(materialBuffer);

    resourceManager.destroyBuffer(meshBoundsBuffer);

    for (int i = 0; i < FRAME_OVERLAP; ++i) {
        resourceManager.destroyBuffer(opaqueDrawIndirectBuffers[i]);
        resourceManager.destroyBuffer(transparentDrawIndirectBuffers[i]);
        resourceManager.destroyBuffer(addressBuffers[i]);
        resourceManager.destroyBuffer(primitiveDataBuffers[i]);
        resourceManager.destroyBuffer(modelMatrixBuffers[i]);
        resourceManager.destroyBuffer(opaqueCullingAddressBuffers[i]);
        resourceManager.destroyBuffer(transparentCullingAddressBuffers[i]);
    }

    resourceManager.destroyDescriptorBuffer(addressesDescriptorBuffer);
    resourceManager.destroyDescriptorBuffer(frustumCullingDescriptorBuffer);
    resourceManager.destroyDescriptorBuffer(textureDescriptorBuffer);

    opaqueDrawCommands.clear();
    transparentDrawCommands.clear();

    materials.clear();
    vertexPositions.clear();
    vertexProperties.clear();
    indices.clear();
    meshes.clear();
    boundingSpheres.clear();
    renderNodes.clear();
    topNodes.clear();


    bIsLoaded = false;
}

uint32_t RenderObject::getFreePrimitiveIndex()
{
    if (freePrimitiveIndices.empty()) {
        const size_t oldSize = currentPrimitiveCount;
        const size_t newSize = currentPrimitiveCount + 50;
        freePrimitiveIndices.reserve(newSize);
        for (int32_t i = oldSize; i < newSize; ++i) { freePrimitiveIndices.insert(i); }
        currentPrimitiveCount = newSize;
        dirty();
    }

    assert(!freePrimitiveIndices.empty());
    const uint32_t index = *freePrimitiveIndices.begin();
    freePrimitiveIndices.erase(freePrimitiveIndices.begin());
    return index;
}

uint32_t RenderObject::getFreeInstanceIndex()
{
    if (freeInstanceIndices.empty()) {
        const size_t oldSize = currentInstanceCount;
        const size_t newSize = currentInstanceCount + 10;
        freeInstanceIndices.reserve(newSize);
        for (int32_t i = oldSize; i < newSize; ++i) { freeInstanceIndices.insert(i); }
        currentInstanceCount = newSize;
        dirty();
    }

    assert(!freeInstanceIndices.empty());
    const uint32_t index = *freeInstanceIndices.begin();
    freeInstanceIndices.erase(freeInstanceIndices.begin());
    return index;
}
}
