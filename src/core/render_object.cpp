//
// Created by William on 2024-08-24.
//

#include "render_object.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

RenderObject::RenderObject(Engine* engine, std::string_view gltfFilepath)
{
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


    // Textures
    std::vector<AllocatedImage> images;
    constexpr uint32_t imageOffset = 1; // default texture at 0
    assert(Engine::whiteImage.image != VK_NULL_HANDLE);
    images.push_back(Engine::whiteImage);
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

    // Materials
    std::vector<Material> materials;
    uint32_t materialOffset = 1; // default material at 0
    materials.emplace_back();
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

            vk_helpers::loadTexture(gltfMaterial.pbrData.baseColorTexture, gltf, material.textureImageIndices.x, material.textureSamplerIndices.x);
            vk_helpers::loadTexture(gltfMaterial.pbrData.metallicRoughnessTexture, gltf, material.textureImageIndices.y, material.textureSamplerIndices.y);

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

    std::vector<Mesh> meshes;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (fastgltf::Mesh& mesh: gltf.meshes) {
            indices.clear();
            vertices.clear();
            bool hasTransparentPrimitives = false;
            for (auto&& p: mesh.primitives) {
                size_t initial_vtx = vertices.size();
                size_t materialIndex = 0;

                /*if (p.materialIndex.has_value()) {
                    materialIndex = p.materialIndex.value() + 1;
                    MaterialPass matType = static_cast<MaterialPass>(file.materials[materialIndex].alphaCutoff.y);
                    if (matType == MaterialPass::Transparent) { hasTransparentPrimitives = true; }
                }*/


                // load indexes
                {
                    fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                    indices.reserve(indices.size() + indexaccessor.count);

                    fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor, [&](std::uint32_t idx) {
                        indices.push_back(idx + static_cast<uint32_t>(initial_vtx));
                    });
                }

                // load vertex positions
                {
                    // position is a REQUIRED property in gltf. No need to check if it exists
                    fastgltf::Attribute* positionIt = p.findAttribute("POSITION");
                    fastgltf::Accessor& posAccessor = gltf.accessors[positionIt->accessorIndex];
                    vertices.resize(vertices.size() + posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, posAccessor, [&](fastgltf::math::fvec3 v, size_t index) {
                        Vertex newvtx{};
                        newvtx.position = {v.x(), v.y(), v.z()};
                        /*newvtx.normal = {1, 0, 0}; // default init the rest of the values
                        newvtx.color = glm::vec4{1.f};
                        newvtx.uv = glm::vec2(0, 0);
                        newvtx.materialIndex = static_cast<uint32_t>(materialIndex);*/
                        vertices[initial_vtx + index] = newvtx;
                    });
                }

                // load vertex normals
                fastgltf::Attribute* normals = p.findAttribute("NORMAL");
                if (normals != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex], [&](fastgltf::math::fvec3 n, size_t index) {
                        //vertices[initial_vtx + index].normal = {n.x(), n.y(), n.z()};
                    });
                }

                // load UVs
                fastgltf::Attribute* uvs = p.findAttribute("TEXCOORD_0");
                if (uvs != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uvs->accessorIndex], [&](fastgltf::math::fvec2 uv, size_t index) {
                        //vertices[initial_vtx + index].uv = { uv.x(), uv.y() };
                    });
                }

                // load vertex colors
                fastgltf::Attribute* colors = p.findAttribute("COLOR_0");
                if (colors != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex], [&](fastgltf::math::fvec4 color, size_t index) {
                        //vertices[initial_vtx + index].color = {color.x(), color.y(), color.z(), color.w()};
                    });
                }
            }

            meshes.push_back({ vertices, indices });
        }
}

void RenderObject::BindDescriptors(VkCommandBuffer cmd)
{}
