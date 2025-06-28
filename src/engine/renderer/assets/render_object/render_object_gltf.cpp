//
// Created by William on 2025-01-24.
//

#include "render_object_gltf.h"

#include <fastgltf/include/fastgltf/core.hpp>
#include <fastgltf/include/fastgltf/types.hpp>
#include <fastgltf/include/fastgltf/tools.hpp>
#include <fmt/include/fmt/format.h>
#include <vulkan/vulkan_core.h>

#include "engine/renderer/vulkan_context.h"
#include "render_object_constants.h"
#include "engine/core/game_object/game_object_factory.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/resources/buffer.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_types.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_uniform.h"
#include "engine/util/file.h"
#include "engine/util/model_utils.h"

namespace will_engine::renderer
{
RenderObjectGltf::RenderObjectGltf(ResourceManager& resourceManager, const RenderObjectInfo& renderObjectInfo)
    : RenderObject(resourceManager, renderObjectInfo)
{}

RenderObjectGltf::~RenderObjectGltf()
{
    unload();
}

void RenderObjectGltf::update(VkCommandBuffer cmd, const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    if (!bIsLoaded) { return; }
    updateBuffers(cmd, currentFrameOverlap);

    for (const std::pair renderablePair : renderableMap) {
        IRenderable* renderable = renderablePair.first;
        if (renderable->getRenderFramesToUpdate() > 0) {
            const uint32_t instanceIndex = renderablePair.second.modelIndex;

            if (instanceIndex >= currentMaxModelCount) {
                fmt::print("Instance from renderable is not a valid index");
                continue;
            }

            const BufferPtr& previousFrameModelMatrix = modelMatrixBuffers[previousFrameOverlap];
            const BufferPtr& currentFrameModelMatrix = modelMatrixBuffers[currentFrameOverlap];

            if (!currentFrameModelMatrix || !previousFrameModelMatrix) {
                fmt::print("Render Object model matrix buffer not found");
                continue;
            }

            auto prevModel = reinterpret_cast<ModelData*>(static_cast<char*>(previousFrameModelMatrix->info.pMappedData) + instanceIndex * sizeof(
                                                              ModelData));
            auto currentModel = reinterpret_cast<ModelData*>(
                static_cast<char*>(currentFrameModelMatrix->info.pMappedData) + instanceIndex * sizeof(ModelData));

            assert(reinterpret_cast<uintptr_t>(prevModel) % alignof(ModelData) == 0 && "Misaligned instance data access");
            assert(reinterpret_cast<uintptr_t>(currentModel) % alignof(ModelData) == 0 && "Misaligned instance data access");

            currentModel->previousModelMatrix = prevModel->currentModelMatrix;
            currentModel->currentModelMatrix = renderable->getModelMatrix();
            currentModel->flags[0] = renderable->isVisible();
            currentModel->flags[1] = renderable->isShadowCaster();

            renderable->setRenderFramesToUpdate(renderable->getRenderFramesToUpdate() - 1);

            vk_helpers::bufferBarrier(cmd, currentFrameModelMatrix->buffer, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
                                      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_ACCESS_2_UNIFORM_READ_BIT);
        }
    }
}

void RenderObjectGltf::updateBuffers(VkCommandBuffer cmd, const int32_t currentFrameOverlap)
{
    if (!bIsLoaded || bufferFramesToUpdate <= 0) { return; }

    bool bDrawBuffersRecreated = false;
    BufferPtr& currentInstanceBuffer = instanceDataBuffer[currentFrameOverlap];
    BufferPtr& currentInstanceStaging = instanceDataStaging[currentFrameOverlap];


    // Recreate the instance buffer if needed (will never shrink)
    const size_t latestInstanceBufferSize = currentMaxInstanceCount * sizeof(InstanceData);
    if (currentInstanceBuffer->info.size != latestInstanceBufferSize) {
        resourceManager.destroyResource(std::move(currentInstanceBuffer));
        currentInstanceBuffer = resourceManager.createResource<Buffer>(BufferType::Device, latestInstanceBufferSize);
        resourceManager.destroyResource(std::move(currentInstanceStaging));
        currentInstanceStaging = resourceManager.createResource<Buffer>(BufferType::Staging, latestInstanceBufferSize);

        bDrawBuffersRecreated = true;
    }

    BufferPtr& currentModelBuffer = modelMatrixBuffers[currentFrameOverlap];
    const size_t latestModelBufferSize = currentMaxModelCount * sizeof(ModelData);
    if (currentModelBuffer->info.size != latestModelBufferSize) {
        resourceManager.destroyResource(std::move(currentModelBuffer));
        currentModelBuffer = resourceManager.createResource<Buffer>(BufferType::HostRandom, latestModelBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        bDrawBuffersRecreated = true;
    }

    // Upload instance data
    memcpy(currentInstanceStaging->info.pMappedData, instanceData.data(), latestInstanceBufferSize);
    vk_helpers::copyBuffer(cmd, currentInstanceStaging->buffer, 0, currentInstanceBuffer->buffer, 0, latestInstanceBufferSize);
    vk_helpers::bufferBarrier(cmd, currentInstanceBuffer->buffer,
                              VK_PIPELINE_STAGE_2_COPY_BIT,
                              VK_ACCESS_2_TRANSFER_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_UNIFORM_READ_BIT);

    size_t requiredBufferSize = sizeof(VkDrawIndexedIndirectCommand) * currentMaxInstanceCount;
    if (compactOpaqueDrawBuffer->info.size != requiredBufferSize) {
        resourceManager.destroyResource(std::move(compactOpaqueDrawBuffer));
        compactOpaqueDrawBuffer = resourceManager.createResource<Buffer>(BufferType::Device, requiredBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        bDrawBuffersRecreated = true;
    }

    if (compactTransparentDrawBuffer->info.size != requiredBufferSize) {
        resourceManager.destroyResource(std::move(compactTransparentDrawBuffer));
        compactTransparentDrawBuffer = resourceManager.createResource<Buffer>(BufferType::Device, requiredBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        bDrawBuffersRecreated = true;
    }

    if (fullShadowDrawBuffer->info.size != requiredBufferSize) {
        resourceManager.destroyResource(std::move(fullShadowDrawBuffer));
        fullShadowDrawBuffer = resourceManager.createResource<Buffer>(BufferType::Device, requiredBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        bDrawBuffersRecreated = true;
    }


    if (bDrawBuffersRecreated) {
        constexpr size_t visPassAddressesSize = sizeof(VisibilityPassBuffers);
        VisibilityPassBuffers visPassData{};
        visPassData.instanceBuffer = resourceManager.getBufferAddress(currentInstanceBuffer->buffer);
        visPassData.modelMatrixBuffer = resourceManager.getBufferAddress(currentModelBuffer->buffer);
        visPassData.primitiveDataBuffer = resourceManager.getBufferAddress(primitiveBuffer->buffer);
        visPassData.opaqueIndirectBuffer = resourceManager.getBufferAddress(compactOpaqueDrawBuffer->buffer);
        visPassData.transparentIndirectBuffer = resourceManager.getBufferAddress(compactTransparentDrawBuffer->buffer);
        visPassData.shadowIndirectBuffer = resourceManager.getBufferAddress(fullShadowDrawBuffer->buffer);
        visPassData.countBuffer = resourceManager.getBufferAddress(countBuffers[currentFrameOverlap]->buffer);
        memcpy(visibilityPassBuffers[currentFrameOverlap]->info.pMappedData, &visPassData, visPassAddressesSize);

        constexpr size_t mainDrawAddressesSize = sizeof(MainDrawBuffers);
        MainDrawBuffers mainDrawData{};
        mainDrawData.instanceBuffer = resourceManager.getBufferAddress(currentInstanceBuffer->buffer);
        mainDrawData.modelBufferAddress = resourceManager.getBufferAddress(currentModelBuffer->buffer);
        mainDrawData.primitiveDataBuffer = resourceManager.getBufferAddress(primitiveBuffer->buffer);
        //todo: multi-material buffer for runtime-modification
        mainDrawData.materialBuffer = resourceManager.getBufferAddress(materialBuffer->buffer);
        memcpy(addressBuffers[currentFrameOverlap]->info.pMappedData, &mainDrawData, mainDrawAddressesSize);
    }

    bufferFramesToUpdate--;
}

void RenderObjectGltf::dirty()
{
    bufferFramesToUpdate = FRAME_OVERLAP;
    // Also marks renderables as dirty because the data needs to be re-uploaded to the new buffer
    for (const auto key : renderableMap | std::views::keys) {
        key->dirty();
    }
}

void RenderObjectGltf::generateMeshComponents(IComponentContainer* container, const Transform& transform)
{
    if (container == nullptr) {
        fmt::print("Warning: Failed to generate mesh components because container is null");
        return;
    }

    for (const int32_t rootNode : topNodes) {
        recursiveGenerate(renderNodes[rootNode], container, transform);
    }

    dirty();
}

void RenderObjectGltf::recursiveGenerate(const RenderNode& renderNode, IComponentContainer* container, const Transform& parentTransform)
{
    Transform thisNodeTransform = renderNode.transform;
    thisNodeTransform.applyParentTransform(parentTransform);

    if (renderNode.meshIndex != -1) {
        auto newMeshComponent = game::ComponentFactory::getInstance().create(
            game::MeshRendererComponent::getStaticType(), renderNode.name);
        game::Component* component = container->addComponent(std::move(newMeshComponent));
        if (const auto meshComponent = dynamic_cast<game::MeshRendererComponent*>(component)) {
            generateMesh(meshComponent, renderNode.meshIndex);
            meshComponent->setTransform(thisNodeTransform);
        }
    }

    for (const auto& child : renderNode.children) {
        recursiveGenerate(*child, container, thisNodeTransform);
    }
}

void RenderObjectGltf::generateMesh(IRenderable* renderable, const int32_t meshIndex)
{
    if (renderable == nullptr) {
        fmt::print("Warning: Failed to generate mesh from gameobject {} because renderable is nullptr", renderObjectInfo.name.c_str());
        return;
    }
    if (meshIndex < 0 || meshIndex >= meshes.size()) {
        fmt::print("Warning: Failed to generate mesh from gameobject {} because mesh index specified is out of bounds",
                   renderObjectInfo.name.c_str());
        return;
    }

    const uint32_t modelIndex = getFreeModelIndex();

    // All primitives associated with the mesh
    const std::vector<uint32_t>& meshPrimitiveIndices = meshes[meshIndex].primitiveIndices;
    for (const uint32_t primitiveIndex : meshPrimitiveIndices) {
        const uint32_t instanceIndex = getFreeInstanceIndex();
        InstanceData& instanceDatum = instanceData[instanceIndex];
        instanceDatum.modelIndex = modelIndex;
        instanceDatum.primitiveDataIndex = primitiveIndex;
        instanceDatum.bIsBeingDrawn = 1;
    }

    renderable->setRenderObjectReference(this, meshIndex);

    RenderableProperties renderableProperties{
        modelIndex
    };
    renderableMap.insert({renderable, renderableProperties});

    dirty();
}

bool RenderObjectGltf::releaseInstanceIndex(IRenderable* renderable)
{
    const auto it = renderableMap.find(renderable);
    if (it == renderableMap.end()) {
        fmt::print("WARNING: Render object instructed to release instance index when it is already free (not found in map)");
        return false;
    }

    const RenderableProperties& renderableProperties = it->second;
    const uint32_t renderableModelIndex = renderableProperties.modelIndex;

    for (int32_t i = 0; i < instanceData.size(); ++i) {
        if (instanceData[i].modelIndex == renderableModelIndex) {
            instanceData[i].modelIndex = -1;
            instanceData[i].primitiveDataIndex = -1;
            instanceData[i].bIsBeingDrawn = 0;

            freeInstanceIndices.push_back(i);
        }
    }


    freeModelIndex.push_back(renderableModelIndex);
    renderableMap.erase(renderable);

    dirty();
    return true;
}

std::vector<Primitive> RenderObjectGltf::getPrimitives(const int32_t meshIndex)
{
#if WILL_ENGINE_DEBUG
    const std::vector<uint32_t>& meshPrimitiveIndices = meshes[meshIndex].primitiveIndices;
    std::vector<Primitive> primitives;
    primitives.reserve(meshPrimitiveIndices.size()); // Reserve space, don't create objects

    for (const uint32_t primitiveIndex : meshPrimitiveIndices) {
        primitives.push_back(debugPrimitives[primitiveIndex]);
    }
    return primitives;
#else
    return {};
#endif
}

bool RenderObjectGltf::parseGltf(const std::filesystem::path& gltfFilepath, std::vector<MaterialProperties>& materials, std::vector<VertexPosition>& vertexPositions,
                                 std::vector<VertexProperty>& vertexProperties, std::vector<uint32_t>& indices, std::vector<Primitive>& primitives)
{
    auto start = std::chrono::system_clock::now();

    fastgltf::Parser parser{
        fastgltf::Extensions::KHR_texture_basisu | fastgltf::Extensions::KHR_mesh_quantization | fastgltf::Extensions::KHR_texture_transform
    };
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
        primitives.reserve(primitives.capacity() + mesh.primitives.size());
#if WILL_ENGINE_DEBUG
        debugPrimitives.reserve(debugPrimitives.capacity() + mesh.primitives.size());;
#endif
        meshData.primitiveIndices.reserve(mesh.primitives.size());

        for (fastgltf::Primitive& p : mesh.primitives) {
            Primitive primitiveData{};

            std::vector<VertexPosition> primitiveVertexPositions{};
            std::vector<VertexProperty> primitiveVertexProperties{};
            std::vector<uint32_t> primitiveIndices{};

            if (p.materialIndex.has_value()) {
                primitiveData.materialIndex = p.materialIndex.value() + materialOffset;
                primitiveData.bHasTransparent = (static_cast<MaterialType>(materials[primitiveData.materialIndex].alphaProperties.y) ==
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
                const fastgltf::Accessor& uvAccessor = gltf.accessors[uvs->accessorIndex];

                switch (uvAccessor.componentType) {
                    case fastgltf::ComponentType::Byte:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::s8vec2>(gltf, uvAccessor,
                                                                                   [&](fastgltf::math::s8vec2 uv, const size_t index) {
                                                                                       // f = max(c / 127.0, -1.0)
                                                                                       float u = std::max(static_cast<float>(uv.x()) / 127.0f, -1.0f);
                                                                                       float v = std::max(static_cast<float>(uv.y()) / 127.0f, -1.0f);
                                                                                       primitiveVertexProperties[index].uv = {u, v};
                                                                                   });
                        break;
                    case fastgltf::ComponentType::UnsignedByte:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u8vec2>(gltf, uvAccessor,
                                                                                   [&](fastgltf::math::u8vec2 uv, const size_t index) {
                                                                                       // f = c / 255.0
                                                                                       float u = static_cast<float>(uv.x()) / 255.0f;
                                                                                       float v = static_cast<float>(uv.y()) / 255.0f;
                                                                                       primitiveVertexProperties[index].uv = {u, v};
                                                                                   });
                        break;
                    case fastgltf::ComponentType::Short:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::s16vec2>(gltf, uvAccessor,
                                                                                    [&](fastgltf::math::s16vec2 uv, const size_t index) {
                                                                                        // f = max(c / 32767.0, -1.0)
                                                                                        float u = std::max(
                                                                                            static_cast<float>(uv.x()) / 32767.0f, -1.0f);
                                                                                        float v = std::max(
                                                                                            static_cast<float>(uv.y()) / 32767.0f, -1.0f);
                                                                                        primitiveVertexProperties[index].uv = {u, v};
                                                                                    });
                        break;
                    case fastgltf::ComponentType::UnsignedShort:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec2>(gltf, uvAccessor,
                                                                                    [&](fastgltf::math::u16vec2 uv, const size_t index) {
                                                                                        // f = c / 65535.0
                                                                                        float u = static_cast<float>(uv.x()) / 65535.0f;
                                                                                        float v = static_cast<float>(uv.y()) / 65535.0f;
                                                                                        primitiveVertexProperties[index].uv = {u, v};
                                                                                    });
                        break;
                    case fastgltf::ComponentType::Float:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, uvAccessor,
                                                                                  [&](fastgltf::math::fvec2 uv, const size_t index) {
                                                                                      primitiveVertexProperties[index].uv = {uv.x(), uv.y()};
                                                                                  });
                        break;
                    default:
                        fmt::print("Unsupported UV component type: {}\n", static_cast<int>(uvAccessor.componentType));
                        break;
                }
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

            primitiveData.firstIndex = static_cast<uint32_t>(indices.size());
            primitiveData.vertexOffset = static_cast<int32_t>(vertexPositions.size());
            primitiveData.indexCount = static_cast<uint32_t>(primitiveIndices.size());
            primitiveData.boundingSphere = BoundingSphere::getBounds(primitiveVertexPositions);

            vertexPositions.insert(vertexPositions.end(), primitiveVertexPositions.begin(), primitiveVertexPositions.end());
            vertexProperties.insert(vertexProperties.end(), primitiveVertexProperties.begin(), primitiveVertexProperties.end());
            indices.insert(indices.end(), primitiveIndices.begin(), primitiveIndices.end());

            meshData.primitiveIndices.push_back(primitives.size());
#if WILL_ENGINE_DEBUG
            debugPrimitives.push_back(primitiveData);
#endif
            primitives.push_back(primitiveData);
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

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    float time = static_cast<float>(elapsed.count()) / 1000000.0f;
    fmt::print("GLTF: {} | Sampl: {} | Imag: {} | Mats: {} | Mesh: {} | Prim: {} | Inst: {} | in {}ms\n",
               file::getFileName(gltfFilepath.filename().string().c_str()),
               samplers.size(), images.size(), materials.size() - materialOffset, meshes.size(), primitives.size(), instanceCount, time);
    return true;
}

void RenderObjectGltf::load()
{
    if (bIsLoaded) {
        fmt::print("Render Object attempted to load when it is already loaded");
        return;
    }

    freeModelIndex.reserve(currentMaxModelCount);
    for (int32_t i = 0; i < currentMaxModelCount; ++i) { freeModelIndex.push_back(i); }

    freeInstanceIndices.reserve(currentMaxInstanceCount);
    for (int32_t i = 0; i < currentMaxInstanceCount; ++i) { freeInstanceIndices.push_back(i); }
    instanceData.resize(currentMaxInstanceCount);

    std::vector<MaterialProperties> materials{};
    std::vector<VertexPosition> vertexPositions{};
    std::vector<VertexProperty> vertexProperties{};
    std::vector<uint32_t> indices{};
    std::vector<Primitive> primitives{};

    if (!parseGltf(renderObjectInfo.sourcePath, materials, vertexPositions, vertexProperties, indices, primitives)) { return; }

    std::vector<DescriptorImageData> textureDescriptors;
    // 0 is always a "fallback sampler"
    textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, {.sampler = resourceManager.getDefaultSamplerNearest()}, false});
    for (const SamplerPtr& sampler : samplers) {
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
    for (ImageResourcePtr& image : images) {
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

    uint64_t primitiveBufferSize = sizeof(Primitive) * primitives.size();
    BufferPtr primitiveBufferStaging = resourceManager.createResource<Buffer>(BufferType::Staging, primitiveBufferSize);
    memcpy(primitiveBufferStaging->info.pMappedData, primitives.data(), primitiveBufferSize);
    primitiveBuffer = resourceManager.createResource<Buffer>(BufferType::Device, primitiveBufferSize);

    std::array<BufferCopyInfo, 5> bufferCopies = {
        BufferCopyInfo(materialStaging->buffer, 0, materialBuffer->buffer, 0, materialBufferSize),
        {vertexPositionStaging->buffer, 0, vertexPositionBuffer->buffer, 0, vertexPositionBufferSize},
        {vertexPropertiesStaging->buffer, 0, vertexPropertyBuffer->buffer, 0, vertexPropertiesBufferSize},
        {indexStaging->buffer, 0, indexBuffer->buffer, 0, indicesBufferSize},
        {primitiveBufferStaging->buffer, 0, primitiveBuffer->buffer, 0, primitiveBufferSize},
    };

    resourceManager.copyBufferImmediate(bufferCopies);


    addressesDescriptorBuffer = resourceManager.createResource<DescriptorBufferUniform>(resourceManager.getRenderObjectAddressesLayout(), FRAME_OVERLAP);
    std::array<DescriptorUniformData, 1> addressUniformData{{}};
    for (int32_t i = 0; i < FRAME_OVERLAP; i++) {
        constexpr size_t addressesSize = sizeof(MainDrawBuffers);
        modelMatrixBuffers[i] = resourceManager.createResource<Buffer>(BufferType::HostRandom, currentMaxModelCount * sizeof(ModelData));
        instanceDataBuffer[i] = resourceManager.createResource<Buffer>(BufferType::Device, currentMaxInstanceCount * sizeof(InstanceData));
        instanceDataStaging[i] = resourceManager.createResource<Buffer>(BufferType::Staging, currentMaxInstanceCount * sizeof(InstanceData));

        addressBuffers[i] = resourceManager.createResource<Buffer>(BufferType::HostSequential, addressesSize);
        addressUniformData[0] = {
            .buffer = addressBuffers[i]->buffer,
            .allocSize = addressesSize,
        };
        addressesDescriptorBuffer->setupData(addressUniformData, i);


        MainDrawBuffers mainDrawData{};
        mainDrawData.instanceBuffer = resourceManager.getBufferAddress(instanceDataBuffer[i]->buffer);
        mainDrawData.modelBufferAddress = resourceManager.getBufferAddress(modelMatrixBuffers[i]->buffer);
        mainDrawData.primitiveDataBuffer = resourceManager.getBufferAddress(primitiveBuffer->buffer);
        //todo: multi-material buffer for runtime-modification
        mainDrawData.materialBuffer = resourceManager.getBufferAddress(materialBuffer->buffer);
        memcpy(addressBuffers[i]->info.pMappedData, &mainDrawData, addressesSize);
    }

    size_t maxDrawBufferSize = sizeof(VkDrawIndexedIndirectCommand) * currentMaxInstanceCount;
    compactOpaqueDrawBuffer = resourceManager.createResource<Buffer>(BufferType::Device, maxDrawBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    compactTransparentDrawBuffer = resourceManager.createResource<Buffer>(BufferType::Device, maxDrawBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    fullShadowDrawBuffer = resourceManager.createResource<Buffer>(BufferType::Device, maxDrawBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

    visibilityPassDescriptorBuffer = resourceManager.createResource<DescriptorBufferUniform>(resourceManager.getVisibilityPassLayout(), FRAME_OVERLAP);
    std::array<DescriptorUniformData, 1> uniformData{};
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        countBuffers[i] = resourceManager.createResource<Buffer>(BufferType::Device, sizeof(IndirectCount), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);


        constexpr size_t bufferSize = sizeof(VisibilityPassBuffers);
        visibilityPassBuffers[i] = resourceManager.createResource<Buffer>(BufferType::HostSequential, bufferSize);
        uniformData[0] = {
            .buffer = visibilityPassBuffers[i]->buffer,
            .allocSize = bufferSize,
        };
        visibilityPassDescriptorBuffer->setupData(uniformData, i);

        VisibilityPassBuffers visPassData{};
        visPassData.instanceBuffer = resourceManager.getBufferAddress(instanceDataBuffer[i]->buffer);
        visPassData.modelMatrixBuffer = resourceManager.getBufferAddress(modelMatrixBuffers[i]->buffer);
        visPassData.primitiveDataBuffer = resourceManager.getBufferAddress(primitiveBuffer->buffer);
        visPassData.opaqueIndirectBuffer = resourceManager.getBufferAddress(compactOpaqueDrawBuffer->buffer);
        visPassData.transparentIndirectBuffer = resourceManager.getBufferAddress(compactTransparentDrawBuffer->buffer);
        visPassData.shadowIndirectBuffer = resourceManager.getBufferAddress(fullShadowDrawBuffer->buffer);
        visPassData.countBuffer = resourceManager.getBufferAddress(countBuffers[i]->buffer);
        memcpy(visibilityPassBuffers[i]->info.pMappedData, &visPassData, bufferSize);
    }

    resourceManager.destroyResourceImmediate(std::move(materialStaging));
    resourceManager.destroyResourceImmediate(std::move(vertexPositionStaging));
    resourceManager.destroyResourceImmediate(std::move(vertexPropertiesStaging));
    resourceManager.destroyResourceImmediate(std::move(indexStaging));
    resourceManager.destroyResourceImmediate(std::move(primitiveBufferStaging));

    bIsLoaded = true;
    dirty();
}

void RenderObjectGltf::unload()
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

    for (ImageResourcePtr& image : images) {
        resourceManager.destroyResource(std::move(image));
    }
    images.clear();

    for (SamplerPtr& sampler : samplers) {
        resourceManager.destroyResource(std::move(sampler));
    }
    samplers.clear();

    resourceManager.destroyResource(std::move(vertexPositionBuffer));
    resourceManager.destroyResource(std::move(vertexPropertyBuffer));
    resourceManager.destroyResource(std::move(indexBuffer));
    resourceManager.destroyResource(std::move(primitiveBuffer));

    resourceManager.destroyResource(std::move(materialBuffer));

    resourceManager.destroyResource(std::move(compactOpaqueDrawBuffer));
    resourceManager.destroyResource(std::move(compactTransparentDrawBuffer));
    resourceManager.destroyResource(std::move(fullShadowDrawBuffer));

    for (int i = 0; i < FRAME_OVERLAP; ++i) {
        resourceManager.destroyResource(std::move(addressBuffers[i]));
        resourceManager.destroyResource(std::move(instanceDataBuffer[i]));
        resourceManager.destroyResource(std::move(instanceDataStaging[i]));
        resourceManager.destroyResource(std::move(modelMatrixBuffers[i]));
        resourceManager.destroyResource(std::move(visibilityPassBuffers[i]));
        resourceManager.destroyResource(std::move(countBuffers[i]));
    }

    resourceManager.destroyResource(std::move(addressesDescriptorBuffer));
    resourceManager.destroyResource(std::move(textureDescriptorBuffer));
    resourceManager.destroyResource(std::move(visibilityPassDescriptorBuffer));

    meshes.clear();
    renderNodes.clear();
    topNodes.clear();


    bIsLoaded = false;
}

uint32_t RenderObjectGltf::getFreeInstanceIndex()
{
    if (freeInstanceIndices.empty()) {
        const size_t oldSize = currentMaxInstanceCount;
        const size_t newSize = currentMaxInstanceCount + DEFAULT_RENDER_OBJECT_INSTANCE_COUNT;
        freeInstanceIndices.reserve(newSize);
        instanceData.resize(newSize);
        for (int32_t i = oldSize; i < newSize; ++i) { freeInstanceIndices.push_back(i); }
        currentMaxInstanceCount = newSize;
        dirty();
    }

    assert(!freeInstanceIndices.empty());
    const uint32_t index = *freeInstanceIndices.begin();
    freeInstanceIndices.erase(freeInstanceIndices.begin());
    return index;
}

uint32_t RenderObjectGltf::getFreeModelIndex()
{
    if (freeModelIndex.empty()) {
        const size_t oldSize = currentMaxModelCount;
        const size_t newSize = currentMaxModelCount + 10;
        freeModelIndex.reserve(newSize);
        for (int32_t i = oldSize; i < newSize; ++i) { freeModelIndex.push_back(i); }
        currentMaxModelCount = newSize;
        dirty();
    }

    assert(!freeModelIndex.empty());
    const uint32_t index = *freeModelIndex.begin();
    freeModelIndex.erase(freeModelIndex.begin());
    return index;
}
}
