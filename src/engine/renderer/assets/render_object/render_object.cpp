//
// Created by William on 2025-01-24.
//

#include "render_object.h"

#include <fastgltf/include/fastgltf/core.hpp>
#include <fastgltf/include/fastgltf/types.hpp>
#include <fastgltf/include/fastgltf/tools.hpp>
#include <fmt/include/fmt/format.h>
#include <vulkan/vulkan_core.h>

#include "engine/renderer/vulkan_context.h"
#include "render_object_constants.h"
#include "engine/core/game_object/game_object_factory.h"
#include "engine/util/file.h"
#include "engine/util/model_utils.h"

namespace will_engine::renderer
{
RenderObject::RenderObject(ResourceManager& resourceManager, const std::filesystem::path& willmodelPath, const std::filesystem::path& gltfFilepath,
                           std::string name, const uint32_t renderObjectId)
    : willmodelPath(willmodelPath), gltfPath(gltfFilepath), name(std::move(name)), renderObjectId(renderObjectId), resourceManager(resourceManager)
{}

RenderObject::~RenderObject()
{
    unload();
}

void RenderObject::update(VkCommandBuffer cmd, const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    if (!bIsLoaded) { return; }
    updateBuffers(cmd, currentFrameOverlap, previousFrameOverlap);

    for (const std::pair renderablePair : renderableMap) {
        IRenderable* renderable = renderablePair.first;
        if (renderable->getRenderFramesToUpdate() > 0) {
            const uint32_t instanceIndex = renderablePair.second.instanceIndex;

            if (instanceIndex >= currentMaxInstanceCount) {
                fmt::print("Instance from renderable is not a valid index");
                continue;
            }

            const BufferPtr& previousFrameModelMatrix = modelMatrixBuffers[previousFrameOverlap];
            const BufferPtr& currentFrameModelMatrix = modelMatrixBuffers[currentFrameOverlap];

            if (!currentFrameModelMatrix || !previousFrameModelMatrix) {
                fmt::print("Render Object model matrix buffer not found");
                continue;
            }

            auto prevModel = reinterpret_cast<InstanceData*>(static_cast<char*>(previousFrameModelMatrix->info.pMappedData) + instanceIndex * sizeof(
                                                                 InstanceData));
            auto currentModel = reinterpret_cast<InstanceData*>(
                static_cast<char*>(currentFrameModelMatrix->info.pMappedData) + instanceIndex * sizeof(InstanceData));

            assert(reinterpret_cast<uintptr_t>(prevModel) % alignof(InstanceData) == 0 && "Misaligned instance data access");
            assert(reinterpret_cast<uintptr_t>(currentModel) % alignof(InstanceData) == 0 && "Misaligned instance data access");

            currentModel->previousModelMatrix = prevModel->currentModelMatrix;
            currentModel->currentModelMatrix = renderable->getModelMatrix();
            currentModel->flags[0] = renderable->isVisible();
            currentModel->flags[1] = renderable->isShadowCaster();

            renderable->setRenderFramesToUpdate(renderable->getRenderFramesToUpdate() - 1);

            vk_helpers::uniformBarrier(cmd, currentFrameModelMatrix->buffer, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
                                       VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_ACCESS_2_UNIFORM_READ_BIT);
        }
    }
}

bool RenderObject::updateBuffers(VkCommandBuffer cmd, const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    if (bufferFramesToUpdate <= 0) { return true; }

    BufferPtr& currentPrimitiveBuffer = primitiveDataBuffers[currentFrameOverlap];

    if (!currentPrimitiveBuffer) {
        fmt::print("Render Object primitive data buffer not found");
        return false;
    }

    // Recreate the primitive buffer if needed
    size_t latestPrimitiveBufferSize = currentMaxPrimitiveCount * sizeof(PrimitiveData);
    if (currentPrimitiveBuffer->info.size != latestPrimitiveBufferSize) {
        resourceManager.destroyResource(std::move(currentPrimitiveBuffer));
        primitiveDataBuffers[currentFrameOverlap] = resourceManager.createResource<Buffer>(BufferType::HostRandom, latestPrimitiveBufferSize,
                                                                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        const VkDeviceAddress primitiveBufferAddress = resourceManager.getBufferAddress(currentPrimitiveBuffer->buffer);
        memcpy(static_cast<char*>(addressBuffers[currentFrameOverlap]->info.pMappedData) + sizeof(VkDeviceAddress), &primitiveBufferAddress,
               sizeof(VkDeviceAddress));
    }

    // Upload primitive data
    {
        for (auto& pair : primitiveDataMap) {
            auto mappedData = static_cast<char*>(currentPrimitiveBuffer->info.pMappedData);
            auto pPrimitiveData = reinterpret_cast<PrimitiveData*>(mappedData + sizeof(PrimitiveData) * pair.first);

            *pPrimitiveData = pair.second;
        }
        vk_helpers::uniformBarrier(cmd, currentPrimitiveBuffer->buffer, VK_PIPELINE_STAGE_2_HOST_BIT
                                   , VK_ACCESS_2_HOST_WRITE_BIT
                                   , VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                                   , VK_ACCESS_2_UNIFORM_READ_BIT);
    }

    BufferPtr& currentInstanceBuffer = modelMatrixBuffers[currentFrameOverlap];

    // sizes don't match, need to recreate the buffer
    size_t latestInstanceBufferSize = currentMaxInstanceCount * sizeof(InstanceData);
    if (currentInstanceBuffer->info.size != latestInstanceBufferSize) {
        resourceManager.destroyResource(std::move(currentInstanceBuffer));
        currentInstanceBuffer = resourceManager.createResource<Buffer>(BufferType::HostRandom, latestInstanceBufferSize,
                                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        const VkDeviceAddress instanceBufferAddress = resourceManager.getBufferAddress(currentInstanceBuffer->buffer);
        memcpy(static_cast<char*>(addressBuffers[currentFrameOverlap]->info.pMappedData) + sizeof(VkDeviceAddress) * 2, &instanceBufferAddress,
               sizeof(VkDeviceAddress));
    }

    // instance records could be stale,  need to ensure that it cleared/invalidated on GPU
    for (const uint32_t freeIndex : freeInstanceIndices) {
        auto targetModel = reinterpret_cast<InstanceData*>(static_cast<char*>(currentInstanceBuffer->info.pMappedData) + freeIndex * sizeof(
                                                               InstanceData));

        assert(reinterpret_cast<uintptr_t>(targetModel) % alignof(InstanceData) == 0 && "Misaligned instance data access");

        targetModel->previousModelMatrix = glm::identity<glm::mat4>();
        targetModel->currentModelMatrix = glm::identity<glm::mat4>();
        targetModel->flags = glm::vec4(0.0f);
    }

    BufferPtr& currentOpaqueDrawIndirectBuffer = opaqueDrawIndirectBuffers[currentFrameOverlap];
    resourceManager.destroyResource(std::move(currentOpaqueDrawIndirectBuffer));
    if (!opaqueDrawCommands.empty()) {
        resourceManager.destroyResource(std::move(currentOpaqueDrawIndirectBuffer));
        BufferPtr indirectStaging = resourceManager.createResource<Buffer>(
            BufferType::Staging,
            opaqueDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));

        memcpy(indirectStaging->info.pMappedData, opaqueDrawCommands.data(), opaqueDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));

        currentOpaqueDrawIndirectBuffer = resourceManager.createResource<Buffer>(
            BufferType::Device,
            opaqueDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

        vk_helpers::copyBuffer(cmd, indirectStaging->buffer, 0, currentOpaqueDrawIndirectBuffer->buffer, 0,
                               opaqueDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        resourceManager.destroyResource(std::move(indirectStaging));

        const FrustumCullingBuffers cullingAddresses{
            .meshBoundsBuffer = resourceManager.getBufferAddress(meshBoundsBuffer->buffer),
            .commandBuffer = resourceManager.getBufferAddress(currentOpaqueDrawIndirectBuffer->buffer),
            .commandBufferCount = static_cast<uint32_t>(opaqueDrawCommands.size()),
            .padding = {},
        };

        const BufferPtr& currentCullingAddressBuffers = opaqueCullingAddressBuffers[currentFrameOverlap];
        BufferPtr stagingCullingAddressesBuffer = resourceManager.createResource<Buffer>(
            BufferType::Staging, sizeof(FrustumCullingBuffers));
        memcpy(stagingCullingAddressesBuffer->info.pMappedData, &cullingAddresses, sizeof(FrustumCullingBuffers));
        vk_helpers::copyBuffer(cmd, stagingCullingAddressesBuffer->buffer, 0, currentCullingAddressBuffers->buffer, 0, sizeof(FrustumCullingBuffers));
        resourceManager.destroyResource(std::move(stagingCullingAddressesBuffer));
    }

    BufferPtr& currentTransparentDrawIndirectBuffer = transparentDrawIndirectBuffers[currentFrameOverlap];
    resourceManager.destroyResource(std::move(currentTransparentDrawIndirectBuffer));

    if (!transparentDrawCommands.empty()) {
        BufferPtr indirectStaging = resourceManager.createResource<Buffer>(
            BufferType::Staging,
            transparentDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));

        memcpy(indirectStaging->info.pMappedData, transparentDrawCommands.data(),
               transparentDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));

        currentTransparentDrawIndirectBuffer = resourceManager.createResource<Buffer>(
            BufferType::Device,
            transparentDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

        vk_helpers::copyBuffer(cmd, indirectStaging->buffer, 0, currentTransparentDrawIndirectBuffer->buffer, 0,
                               transparentDrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        resourceManager.destroyResource(std::move(indirectStaging));

        const FrustumCullingBuffers cullingAddresses{
            .meshBoundsBuffer = resourceManager.getBufferAddress(meshBoundsBuffer->buffer),
            .commandBuffer = resourceManager.getBufferAddress(currentTransparentDrawIndirectBuffer->buffer),
            .commandBufferCount = static_cast<uint32_t>(transparentDrawCommands.size()),
            .padding = {},
        };

        BufferPtr stagingCullingAddressesBuffer = resourceManager.createResource<Buffer>(
            BufferType::Staging,
            sizeof(FrustumCullingBuffers));
        memcpy(stagingCullingAddressesBuffer->info.pMappedData, &cullingAddresses, sizeof(FrustumCullingBuffers));

        const BufferPtr& currentCullingAddressBuffers = transparentCullingAddressBuffers[currentFrameOverlap];

        vk_helpers::copyBuffer(cmd, stagingCullingAddressesBuffer->buffer, 0, currentCullingAddressBuffers->buffer, 0, sizeof(FrustumCullingBuffers));
        resourceManager.destroyResource(std::move(stagingCullingAddressesBuffer));
    }

    bufferFramesToUpdate--;
    return false;
}

void RenderObject::dirty()
{
    bufferFramesToUpdate = FRAME_OVERLAP;
    for (const auto key : renderableMap | std::views::keys) {
        key->dirty();
    }
}

bool RenderObject::generateMeshComponents(IComponentContainer* container, const Transform& transform)
{
    if (container == nullptr) { return false; }

    for (const int32_t rootNode : topNodes) {
        recursiveGenerate(renderNodes[rootNode], container, transform);
    }

    dirty();
    return true;
}

void RenderObject::recursiveGenerate(const RenderNode& renderNode, IComponentContainer* container)
{
    if (renderNode.meshIndex != -1) {
        auto newMeshComponent = components::ComponentFactory::getInstance().createComponent(
            components::MeshRendererComponent::getStaticType(), renderNode.name);
        components::Component* component = container->addComponent(std::move(newMeshComponent));
        if (const auto meshComponent = dynamic_cast<components::MeshRendererComponent*>(component)) {
            generateMesh(meshComponent, renderNode.meshIndex);
            meshComponent->setTransform(renderNode.transform);
        }
    }

    for (const auto& child : renderNode.children) {
        recursiveGenerate(*child, container);
    }
}

void RenderObject::recursiveGenerate(const RenderNode& renderNode, IComponentContainer* container, const Transform& parentTransform)
{
    Transform thisNodeTransform = renderNode.transform;
    thisNodeTransform.applyParentTransform(parentTransform);

    if (renderNode.meshIndex != -1) {
        auto newMeshComponent = components::ComponentFactory::getInstance().createComponent(
            components::MeshRendererComponent::getStaticType(), renderNode.name);
        components::Component* component = container->addComponent(std::move(newMeshComponent));
        if (const auto meshComponent = dynamic_cast<components::MeshRendererComponent*>(component)) {
            generateMesh(meshComponent, renderNode.meshIndex);
            meshComponent->setTransform(thisNodeTransform);
        }
    }

    for (const auto& child : renderNode.children) {
        recursiveGenerate(*child, container, thisNodeTransform);
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

    freeInstanceIndices.insert(it->second.instanceIndex);

    renderableMap.erase(renderable);
    dirty();
    return true;
}

std::optional<std::reference_wrapper<const Mesh> > RenderObject::getMeshData(const int32_t meshIndex)
{
    if (meshIndex < 0 || meshIndex >= meshes.size()) { return std::nullopt; }
    return meshes[meshIndex];
}

bool RenderObject::parseGltf(const std::filesystem::path& gltfFilepath, std::vector<MaterialProperties>& materials,
                             std::vector<VertexPosition>& vertexPositions, std::vector<VertexProperty>& vertexProperties,
                             std::vector<uint32_t>& indices)
{
    auto start = std::chrono::system_clock::now();

    fastgltf::Parser parser{fastgltf::Extensions::KHR_texture_basisu | fastgltf::Extensions::KHR_mesh_quantization};
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
    for (const fastgltf::Sampler& gltfSampler : gltf.samplers) {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0;

        samplerInfo.magFilter = model_utils::extractFilter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = model_utils::extractFilter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));


        samplerInfo.mipmapMode = model_utils::extractMipMapMode(gltfSampler.minFilter.value_or(fastgltf::Filter::Linear));

        samplers.push_back(resourceManager.createResource<Sampler>(samplerInfo));
    }

    assert(samplers.size() <= render_object_constants::MAX_SAMPLER_COUNT);

    images.reserve(gltf.images.size() + model_utils::imageOffset);
    for (const fastgltf::Image& gltfImage : gltf.images) {
        ImageResourcePtr newImage = model_utils::loadImage(resourceManager, gltf, gltfImage, gltfFilepath.parent_path());
        images.push_back(std::move(newImage));
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
               samplers.size(),
               images.size(), materials.size() - materialOffset, meshes.size(), primitiveCount, instanceCount, time);
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
    currentMaxInstanceCount = freeInstanceIndices.size();

    freePrimitiveIndices.reserve(10);
    for (int32_t i = 0; i < 10; ++i) { freePrimitiveIndices.insert(i); }
    currentMaxPrimitiveCount = freePrimitiveIndices.size();

    std::vector<MaterialProperties> materials{};
    std::vector<VertexPosition> vertexPositions{};
    std::vector<VertexProperty> vertexProperties{};
    std::vector<uint32_t> indices{};

    if (!parseGltf(gltfPath, materials, vertexPositions, vertexProperties, indices)) { return; }

    std::vector<DescriptorImageData> textureDescriptors;
    // 0 is always a "fallback sampler"
    textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, {.sampler = resourceManager.getDefaultSamplerNearest()}, false});
    for (SamplerPtr sampler : samplers) {
        if (!sampler) {
            textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, {.sampler = resourceManager.getDefaultSamplerNearest()}, false});
        }
        else {
            textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, {.sampler = sampler->sampler}, false});
        }
    }

    const size_t remaining = render_object_constants::MAX_SAMPLER_COUNT - samplers.size() - model_utils::samplerOffset;
    for (int i = 0; i < remaining; i++) {
        textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, {}, true});
    }


    // 0 is always a "fallback image view"
    textureDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, {.imageView = resourceManager.getWhiteImage(), .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        false
    });
    for (ImageResourcePtr image : images) {
        if (!image) {
            textureDescriptors.push_back({
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                {.imageView = resourceManager.getErrorCheckerboardImage(), .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, false
            });
        }
        else {
            textureDescriptors.push_back({
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, {.imageView = image->imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, false
            });
        }
    }

    textureDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(resourceManager.getTexturesLayout(), 1);
    textureDescriptorBuffer->setupData(textureDescriptors, 0);


    const uint64_t materialBufferSize = materials.size() * sizeof(MaterialProperties);
    BufferPtr materialStaging = resourceManager.createResource<Buffer>(BufferType::Staging, materialBufferSize);
    memcpy(materialStaging->info.pMappedData, materials.data(), materialBufferSize);
    materialBuffer = resourceManager.createResource<Buffer>(BufferType::Device, materialBufferSize);

    const size_t vertexCount = vertexPositions.size();

    const uint64_t vertexPositionBufferSize = vertexCount * sizeof(VertexPosition);
    BufferPtr vertexPositionStaging = resourceManager.createResource<Buffer>(BufferType::Staging, vertexPositionBufferSize);
    memcpy(vertexPositionStaging->info.pMappedData, vertexPositions.data(), vertexPositionBufferSize);
    vertexPositionBuffer = resourceManager.createResource<Buffer>(BufferType::Device, vertexPositionBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    const uint64_t vertexPropertiesBufferSize = vertexCount * sizeof(VertexProperty);
    BufferPtr vertexPropertiesStaging = resourceManager.createResource<Buffer>(BufferType::Staging, vertexPropertiesBufferSize);
    memcpy(vertexPropertiesStaging->info.pMappedData, vertexProperties.data(), vertexPropertiesBufferSize);
    vertexPropertyBuffer = resourceManager.createResource<Buffer>(BufferType::Device, vertexPropertiesBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    const uint64_t indicesBufferSize = indices.size() * sizeof(uint32_t);
    BufferPtr indexStaging = resourceManager.createResource<Buffer>(BufferType::Staging, indicesBufferSize);
    memcpy(indexStaging->info.pMappedData, indices.data(), indicesBufferSize);
    indexBuffer = resourceManager.createResource<Buffer>(BufferType::Device, indicesBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // Addresses (Texture and Uniform model data)
    // todo: make address buffer for statics (dont need multiple)
    addressesDescriptorBuffer = resourceManager.createResource<DescriptorBufferUniform>(resourceManager.getRenderObjectAddressesLayout(),
                                                                                        FRAME_OVERLAP);
    std::array<DescriptorUniformData, 1> addressUniformData{{}};
    for (int32_t i = 0; i < FRAME_OVERLAP; i++) {
        constexpr size_t addressesSize = sizeof(VkDeviceAddress) * 3;
        addressBuffers[i] = resourceManager.createResource<Buffer>(BufferType::HostSequential, addressesSize);
        DescriptorUniformData addressesUniformData{
            .buffer = addressBuffers[i]->buffer,
            .allocSize = addressesSize,
        };
        addressUniformData[0] = addressesUniformData;
        addressesDescriptorBuffer->setupData(addressUniformData, i);

        modelMatrixBuffers[i] = resourceManager.createResource<Buffer>(BufferType::HostRandom, currentMaxInstanceCount * sizeof(InstanceData),
                                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        primitiveDataBuffers[i] = resourceManager.createResource<Buffer>(BufferType::HostRandom, currentMaxPrimitiveCount * sizeof(PrimitiveData),
                                                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        const VkDeviceAddress materialBufferAddress = resourceManager.getBufferAddress(materialBuffer->buffer);
        const VkDeviceAddress primitiveDataBufferAddress = resourceManager.getBufferAddress(primitiveDataBuffers[i]->buffer);
        const VkDeviceAddress instanceBufferAddress = resourceManager.getBufferAddress(modelMatrixBuffers[i]->buffer);
        const VkDeviceAddress addresses[3] = {materialBufferAddress, primitiveDataBufferAddress, instanceBufferAddress};
        memcpy(addressBuffers[i]->info.pMappedData, addresses, addressesSize);
    }

    frustumCullingDescriptorBuffer = resourceManager.createResource<DescriptorBufferUniform>(
        resourceManager.getFrustumCullLayout(), FRAME_OVERLAP * 2);
    // In 2 Frame Overlap: 0 and 1 for Opaque, 2 and 3 for Transparent
    // Opaque Culling Buffer
    for (int32_t i = 0; i < FRAME_OVERLAP; i++) {
        opaqueCullingAddressBuffers[i] = resourceManager.createResource<Buffer>(BufferType::Device, sizeof(FrustumCullingBuffers));
        const DescriptorUniformData cullingAddressesUniformData{
            .buffer = opaqueCullingAddressBuffers[i]->buffer,
            .allocSize = sizeof(FrustumCullingBuffers),
        };
        addressUniformData[0] = cullingAddressesUniformData;
        frustumCullingDescriptorBuffer->setupData(addressUniformData, i);
    }
    // Transparent Culling Buffer
    for (int32_t i = 0; i < FRAME_OVERLAP; i++) {
        transparentCullingAddressBuffers[i] = resourceManager.createResource<Buffer>(BufferType::Device, sizeof(FrustumCullingBuffers));
        const DescriptorUniformData cullingAddressesUniformData{
            .buffer = transparentCullingAddressBuffers[i]->buffer,
            .allocSize = sizeof(FrustumCullingBuffers),
        };

        addressUniformData[0] = cullingAddressesUniformData;
        frustumCullingDescriptorBuffer->setupData(addressUniformData, FRAME_OVERLAP + i);
    }

    uint64_t boundingSphereBufferSize = sizeof(BoundingSphere) * boundingSpheres.size();
    BufferPtr meshBoundsStaging = resourceManager.createResource<Buffer>(BufferType::Staging, boundingSphereBufferSize);
    memcpy(meshBoundsStaging->info.pMappedData, boundingSpheres.data(), boundingSphereBufferSize);
    meshBoundsBuffer = resourceManager.createResource<Buffer>(BufferType::Device, boundingSphereBufferSize);

    std::array<BufferCopyInfo, 5> bufferCopies = {
        BufferCopyInfo(materialStaging->buffer, 0, materialBuffer->buffer, 0, materialBufferSize),
        {vertexPositionStaging->buffer, 0, vertexPositionBuffer->buffer, 0, vertexPositionBufferSize},
        {vertexPropertiesStaging->buffer, 0, vertexPropertyBuffer->buffer, 0, vertexPropertiesBufferSize},
        {indexStaging->buffer, 0, indexBuffer->buffer, 0, indicesBufferSize},
        {meshBoundsStaging->buffer, 0, meshBoundsBuffer->buffer, 0, boundingSphereBufferSize},
    };

    resourceManager.copyBufferImmediate(bufferCopies);

    // Be careful, this destroys a copy of the staging buffer. Not an issue since it deletes the buffer in GPU memory.
    // Will be dangerous if you attempt to hold onto the staging buffers outside of this load function
    resourceManager.destroyResourceImmediate(std::move(materialStaging));
    resourceManager.destroyResourceImmediate(std::move(vertexPositionStaging));
    resourceManager.destroyResourceImmediate(std::move(vertexPropertiesStaging));
    resourceManager.destroyResourceImmediate(std::move(indexStaging));
    resourceManager.destroyResourceImmediate(std::move(meshBoundsStaging));

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

    for (ImageResourcePtr image : images) {
        resourceManager.destroyResource(std::move(image));
    }

    for (SamplerPtr sampler : samplers) {
        resourceManager.destroyResource(std::move(sampler));
    }

    resourceManager.destroyResource(std::move(vertexPositionBuffer));
    resourceManager.destroyResource(std::move(vertexPropertyBuffer));
    resourceManager.destroyResource(std::move(indexBuffer));

    resourceManager.destroyResource(std::move(materialBuffer));

    resourceManager.destroyResource(std::move(meshBoundsBuffer));

    for (int i = 0; i < FRAME_OVERLAP; ++i) {
        resourceManager.destroyResource(std::move(opaqueDrawIndirectBuffers[i]));
        resourceManager.destroyResource(std::move(transparentDrawIndirectBuffers[i]));
        resourceManager.destroyResource(std::move(addressBuffers[i]));
        resourceManager.destroyResource(std::move(primitiveDataBuffers[i]));
        resourceManager.destroyResource(std::move(modelMatrixBuffers[i]));
        resourceManager.destroyResource(std::move(opaqueCullingAddressBuffers[i]));
        resourceManager.destroyResource(std::move(transparentCullingAddressBuffers[i]));
    }

    resourceManager.destroyResource(std::move(addressesDescriptorBuffer));
    resourceManager.destroyResource(std::move(frustumCullingDescriptorBuffer));
    resourceManager.destroyResource(std::move(textureDescriptorBuffer));

    opaqueDrawCommands.clear();
    transparentDrawCommands.clear();

    meshes.clear();
    boundingSpheres.clear();
    renderNodes.clear();
    topNodes.clear();


    bIsLoaded = false;
}

uint32_t RenderObject::getFreePrimitiveIndex()
{
    if (freePrimitiveIndices.empty()) {
        const size_t oldSize = currentMaxPrimitiveCount;
        const size_t newSize = currentMaxPrimitiveCount + 50;
        freePrimitiveIndices.reserve(newSize);
        for (int32_t i = oldSize; i < newSize; ++i) { freePrimitiveIndices.insert(i); }
        currentMaxPrimitiveCount = newSize;
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
        const size_t oldSize = currentMaxInstanceCount;
        const size_t newSize = currentMaxInstanceCount + 10;
        freeInstanceIndices.reserve(newSize);
        for (int32_t i = oldSize; i < newSize; ++i) { freeInstanceIndices.insert(i); }
        currentMaxInstanceCount = newSize;
        dirty();
    }

    assert(!freeInstanceIndices.empty());
    const uint32_t index = *freeInstanceIndices.begin();
    freeInstanceIndices.erase(freeInstanceIndices.begin());
    return index;
}
}
