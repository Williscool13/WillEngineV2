//
// Created by William on 2025-02-22.
//

#include "model_utils.h"

#include <fmt/format.h>
#include <stb/stb_image.h>
#include <volk/volk.h>
#include <ktx/ktx.h>
#include <ktx/ktxvulkan.h>

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/assets/render_object/render_object.h"

std::optional<AllocatedImage> will_engine::model_utils::loadImage(const ResourceManager& resourceManager, const fastgltf::Asset& asset,
                                                                  const fastgltf::Image& image,
                                                                  const std::filesystem::path& parentFolder)
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

                if (fullPath.extension() == ".ktx" || fullPath.extension() == ".ktx2") {
                    ktxTexture* kTexture;
                    const KTX_error_code ktxResult = ktxTexture_CreateFromNamedFile(fullPath.string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                                                    &kTexture);

                    if (ktxResult == KTX_SUCCESS) {
                        std::optional<VkImageFormatProperties> formatProperties = resourceManager.getPhysicalDeviceImageFormatProperties(
                            ktxTexture_GetVkFormat(kTexture), VK_IMAGE_USAGE_SAMPLED_BIT);
                        if (formatProperties.has_value()) {
                            const unsigned char* data = ktxTexture_GetData(kTexture);
                            VkExtent3D imageExtents{};
                            imageExtents.width = kTexture->baseWidth;
                            imageExtents.height = kTexture->baseHeight;
                            imageExtents.depth = 1;
                            newImage = resourceManager.createImage(data, kTexture->dataSize, imageExtents,
                                                                   ktxTexture_GetVkFormat(kTexture), VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                   false);
                        }
                        else {
                            newImage = resourceManager.getErrorCheckerboardImage();
                        }
                    }

                    //ktxTexture_Destroy(kTexture);
                }
                else {
                    unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 4);
                    if (data) {
                        VkExtent3D imagesize;
                        imagesize.width = width;
                        imagesize.height = height;
                        imagesize.depth = 1;
                        const size_t size = width * height * 4;
                        newImage = resourceManager.createImage(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                        stbi_image_free(data);
                    } else {
                        newImage = resourceManager.getErrorCheckerboardImage();
                    }
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
                                       newImage = resourceManager.createImage(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                                              VK_IMAGE_USAGE_SAMPLED_BIT, true);
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
    }
    else {
        return newImage;
    }
}

will_engine::MaterialProperties will_engine::model_utils::extractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial)
{
    MaterialProperties material = {};
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

void will_engine::model_utils::loadTextureIndices(const fastgltf::Optional<fastgltf::TextureInfo>& texture, const fastgltf::Asset& gltf,
                                                  int& imageIndex, int& samplerIndex)
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

VkFilter will_engine::model_utils::extractFilter(fastgltf::Filter filter)
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

VkSamplerMipmapMode will_engine::model_utils::extractMipMapMode(fastgltf::Filter filter)
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
