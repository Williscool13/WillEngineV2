//
// Created by William on 2024-08-24.
//

#include "render_object.h"

#include "../renderer/vk_descriptors.h"
#include "engine.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "glm/detail/type_quat.hpp"
#include "glm/gtx/matrix_decompose.hpp"

RenderObject::RenderObject(Engine* engine, std::string_view gltfFilepath)
{
    creator = engine;

    // Descriptor Layouts
    {
        {
            DescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            bufferAddressesDescriptorSetLayout = layoutBuilder.build(creator->getDevice(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
                , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        }
        {
            DescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            computeCullingDescriptorSetLayout = layoutBuilder.build(creator->getDevice(), VK_SHADER_STAGE_COMPUTE_BIT
                , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        }
    }

    // Parse GLTF
    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember
                                 | fastgltf::Options::AllowDouble
                                 //| fastgltf::Options::LoadGLBBuffers
                                 | fastgltf::Options::LoadExternalBuffers;

    fastgltf::GltfDataBuffer data;
    data.FromPath(gltfFilepath);
    fastgltf::Asset gltf;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(gltfFilepath);
    if (!static_cast<bool>(gltfFile)) { fmt::print("Failed to open glTF file: {}\n", fastgltf::getErrorMessage(gltfFile.error())); }

    std::filesystem::path path = gltfFilepath;
    auto load = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
    if (!load) { fmt::print("Failed to load glTF: {}\n", fastgltf::to_underlying(load.error())); }

    gltf = std::move(load.get());

    std::vector<VkSampler> samplers;
    constexpr uint32_t samplerOffset = 1; // default sampler at 0
    assert(Engine::defaultSamplerNearest != VK_NULL_HANDLE);
    samplers.push_back(Engine::defaultSamplerNearest);
    // Samplers
    {
        for (const fastgltf::Sampler& gltfSampler: gltf.samplers) {
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


    std::vector<AllocatedImage> images;
    constexpr uint32_t imageOffset = 1; // default texture at 0
    assert(Engine::whiteImage.image != VK_NULL_HANDLE);
    images.push_back(Engine::whiteImage);
    // Images
    {
        for (const fastgltf::Image& gltfImage: gltf.images) {
            std::optional<AllocatedImage> newImage = vk_helpers::loadImage(engine, gltf, gltfImage, path.parent_path());
            if (newImage.has_value()) {
                images.push_back(*newImage);
            } else {
                images.push_back(Engine::errorCheckerboardImage);
                fmt::print("Image failed to load: {}\n", gltfImage.name.c_str());
            }
        }
    }
    assert(images.size() == gltf.images.size() + imageOffset);

    // Binding Images/Samplers
    {
        {
            DescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, samplers.size());
            layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, images.size());

            textureDescriptorSetLayout = layoutBuilder.build(creator->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT
                , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        }

        std::vector<DescriptorImageData> textureDescriptors;
        // samplers
        {
            for (VkSampler sampler : samplers) {
                VkDescriptorImageInfo samplerInfo{.sampler = sampler};
                textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo, false});
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
    uint32_t materialOffset = 1; // default material at 0
    materials.emplace_back();
    //  actual materials
    {
        for (const fastgltf::Material& gltfMaterial: gltf.materials) {
            Material material{};
            material.colorFactor = glm::vec4(
                gltfMaterial.pbrData.baseColorFactor[0]
                , gltfMaterial.pbrData.baseColorFactor[1]
                , gltfMaterial.pbrData.baseColorFactor[2]
                , gltfMaterial.pbrData.baseColorFactor[3]);

            material.metalRoughFactors.x = gltfMaterial.pbrData.metallicFactor;
            material.metalRoughFactors.y = gltfMaterial.pbrData.roughnessFactor;
            material.alphaCutoff.x = 0.5f;

            material.textureImageIndices.x = -1;
            material.textureSamplerIndices.x = -1;
            material.textureImageIndices.y = -1;
            material.textureSamplerIndices.y = -1;


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
                                    material.textureSamplerIndices.x);
            vk_helpers::loadTexture(gltfMaterial.pbrData.metallicRoughnessTexture, gltf, material.textureImageIndices.y,
                                    material.textureSamplerIndices.y);

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

    materialBuffer = engine->createBuffer(materials.size() * sizeof(Material)
        , VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        , VMA_MEMORY_USAGE_CPU_TO_GPU);
    memcpy(materialBuffer.info.pMappedData, materials.data(), materials.size() * sizeof(Material));

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (fastgltf::Mesh& mesh: gltf.meshes) {
        std::vector<Vertex> meshVertices;
        std::vector<uint32_t> meshIndices;
        bool hasTransparentPrimitives = false;
        for (fastgltf::Primitive& p: mesh.primitives) {
            size_t initial_vtx = meshVertices.size();

            if (p.materialIndex.has_value()) {
                size_t materialIndex = p.materialIndex.value() + 1;
                hasTransparentPrimitives = gltf.materials[materialIndex].alphaMode == fastgltf::AlphaMode::Blend;
            }


            // load indexes
            {
                const fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                meshIndices.reserve(meshIndices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor, [&](std::uint32_t idx) {
                    meshIndices.push_back(idx + static_cast<uint32_t>(initial_vtx));
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
                    meshVertices[initial_vtx + index] = newvtx;
                });
            }

            // load vertex normals
            const fastgltf::Attribute* normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex], [&](fastgltf::math::fvec3 n, size_t index) {
                    meshVertices[initial_vtx + index].normal = {n.x(), n.y(), n.z()};
                });
            }

            // load UVs
            const fastgltf::Attribute* uvs = p.findAttribute("TEXCOORD_0");
            if (uvs != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uvs->accessorIndex], [&](fastgltf::math::fvec2 uv, size_t index) {
                    meshVertices[initial_vtx + index].uv = {uv.x(), uv.y()};
                });
            }

            // load vertex colors
            const fastgltf::Attribute* colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex], [&](fastgltf::math::fvec4 color, size_t index) {
                    meshVertices[initial_vtx + index].color = {color.x(), color.y(), color.z(), color.w()};
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
        indices.insert(indices.end(), meshIndices.begin(),  meshIndices.end());
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

                    glm::vec3 translation;
                    glm::vec3 rotation{0.0f};
                    glm::vec3 scale;

                    glm::vec3 skew;
                    glm::vec4 perspective;
                    glm::quat orientation;

                    if (glm::decompose(glmMatrix, scale, orientation, translation, skew, perspective)) {
                        rotation = glm::eulerAngles(orientation);
                    }

                    renderNode.transform = Transform(translation, rotation, scale);
                },
                [&](fastgltf::TRS transform) {
                    const glm::vec3 translation{transform.translation[0], transform.translation[1], transform.translation[2]};
                    const glm::quat quaternionRotation{transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]};
                    const glm::vec3 rotation = glm::eulerAngles(quaternionRotation);
                    const glm::vec3 scale{transform.scale[0], transform.scale[1], transform.scale[2]};


                    renderNode.transform = Transform(translation, rotation, scale);
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
            renderNodes[i].transform.reset();
        }
    }

    UploadIndirect();
}

RenderObject::~RenderObject()
{
    vkDestroyDescriptorSetLayout(creator->getDevice(), bufferAddressesDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(creator->getDevice(), textureDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(creator->getDevice(), computeCullingDescriptorSetLayout, nullptr);

    //vkDestroyPipelineLayout(creator->_device, _computeCullingPipelineLayout, nullptr);
    //vkDestroyPipeline(creator->_device, _computeCullingPipeline, nullptr);

    creator->destroyBuffer(drawIndirectBuffer);
    creator->destroyBuffer(indexBuffer);
    creator->destroyBuffer(vertexBuffer);
    creator->destroyBuffer(materialBuffer);
    textureDescriptorBuffer.destroy(creator->getDevice(), creator->getAllocator());
}

void RenderObject::draw(const VkCommandBuffer& cmd)
{
    if (vertexBuffer.buffer == VK_NULL_HANDLE) {
        return;
    }

    VkDeviceSize offsets{ 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexedIndirect(cmd, drawIndirectBuffer.buffer, 0, drawIndirectCommands.size(),  sizeof(VkDrawIndexedIndirectCommand));
}

GameObject* RenderObject::GenerateGameObject()
{
    auto* superRoot = new GameObject();
    superRoot->setTransform(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
    for (const int32_t rootNode : topNodes) {
        GameObject* root = RecursiveGenerateGameObject(renderNodes[rootNode]);
        superRoot->addChild(root);
    }

    UploadIndirect();
    return superRoot;
}

GameObject* RenderObject::RecursiveGenerateGameObject(const RenderNode& renderNode)
{
    auto gameObject = new GameObject();
    gameObject->setTransform(renderNode.transform);

    if (renderNode.meshIndex != -1) {
        VkDrawIndexedIndirectCommand drawCommand;
        drawCommand.firstIndex = meshes[renderNode.meshIndex].firstIndex;
        drawCommand.indexCount = meshes[renderNode.meshIndex].indexCount;
        drawCommand.vertexOffset = meshes[renderNode.meshIndex].vertexOffset;

        drawCommand.instanceCount = 1;
        drawCommand.firstInstance = drawIndirectCommands.size();

        drawIndirectCommands.push_back(drawCommand);
    }

    for (const RenderNode& node : renderNode.children) {
        GameObject* child = RecursiveGenerateGameObject(node);
        gameObject->addChild(child);
    }
    return gameObject;
}

void RenderObject::UploadIndirect()
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
                                    , VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                                    , VMA_MEMORY_USAGE_GPU_ONLY);

    creator->copyBuffer(indirectStaging, drawIndirectBuffer, drawIndirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
    creator->destroyBuffer(indirectStaging);
}
