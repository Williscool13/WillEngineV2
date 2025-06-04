//
// Created by William on 2025-02-22.
//

// ReSharper disable CppDFAUnreachableCode
#include "model_utils.h"

#include <volk/volk.h>
#include <fmt/format.h>
#include <stb/stb_image.h>
#include <ktx/ktx.h>
#include <ktx/ktxvulkan.h>

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/assets/render_object/render_object.h"
#include "engine/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ambient_occlusion_types.h"
#include "engine/renderer/resources/image_ktx.h"

namespace will_engine::renderer
{
ImageResourcePtr model_utils::loadImage(ResourceManager& resourceManager, const fastgltf::Asset& asset, const fastgltf::Image& image,
                                        const std::filesystem::path& parentFolder)
{
    ImageResourcePtr newImage{};

    int width{}, height{}, nrChannels{};

    std::visit(
        fastgltf::visitor{
            [&](auto& arg) {},
            [&](const fastgltf::sources::URI& fileName) {
                assert(fileName.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(fileName.uri.isLocalPath()); // We're only capable of loading
                // local files.
                const std::wstring widePath(fileName.uri.path().begin(), fileName.uri.path().end());
                const std::filesystem::path fullPath = parentFolder / widePath;

                if (fullPath.extension() == ".ktx") {
                    ktxTexture1* kTexture;
                    const ktx_error_code_e ktxResult = ktxTexture1_CreateFromNamedFile(fullPath.string().c_str(),
                                                                                       KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                                                       &kTexture);

                    if (ktxResult == KTX_SUCCESS) {
                        newImage = processKtxVector(resourceManager, kTexture);
                    }

                    ktxTexture1_Destroy(kTexture);
                }
                else if (fullPath.extension() == ".ktx2") {
                    ktxTexture2* kTexture;
                    const ktx_error_code_e ktxResult = ktxTexture2_CreateFromNamedFile(fullPath.string().c_str(),
                                                                                       KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                                                       &kTexture);

                    if (ktxResult == KTX_SUCCESS) {
                        newImage = processKtxVector(resourceManager, kTexture);
                    }

                    ktxTexture2_Destroy(kTexture);
                }
                else {
                    unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 4);
                    if (data) {
                        VkExtent3D imagesize;
                        imagesize.width = width;
                        imagesize.height = height;
                        imagesize.depth = 1;
                        const size_t size = width * height * 4;
                        newImage = resourceManager.createImageFromData(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                       true);

                        stbi_image_free(data);
                    }
                }
            },
            [&](const fastgltf::sources::Array& vector) {
                if (vector.bytes.size() > 30) {
                    // Minimum size for a meaningful check
                    std::string_view strData(reinterpret_cast<const char*>(vector.bytes.data()),
                                             std::min(size_t(100), vector.bytes.size()));

                    if (strData.find("https://git-lfs.github.com/spec") != std::string_view::npos) {
                        throw std::runtime_error(
                            fmt::format("Git LFS pointer detected instead of actual texture data for image: {}. "
                                        "Please run 'git lfs pull' to retrieve the actual files.",
                                        image.name.c_str()));
                    }
                }

                const int32_t ktxVersion = isKtxTexture(vector);
                switch (ktxVersion) {
                    case 1:
                    {
                        if (validateVector(vector, 0, vector.bytes.size())) {
                            ktxTexture1* kTexture;
                            const ktx_error_code_e ktxResult = ktxTexture1_CreateFromMemory(
                                reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                                vector.bytes.size(),
                                KTX_TEXTURE_CREATE_NO_FLAGS,
                                &kTexture);


                            if (ktxResult == KTX_SUCCESS) {
                                newImage = processKtxVector(resourceManager, kTexture);
                            }

                            ktxTexture1_Destroy(kTexture);
                        }
                        else {
                            fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                        }
                    }
                    break;
                    case 2:
                    {
                        if (validateVector(vector, 0, vector.bytes.size())) {
                            ktxTexture2* kTexture;

                            const KTX_error_code ktxResult = ktxTexture2_CreateFromMemory(
                                reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                                vector.bytes.size(),
                                KTX_TEXTURE_CREATE_NO_FLAGS,
                                &kTexture);

                            if (ktxResult == KTX_SUCCESS) {
                                newImage = processKtxVector(resourceManager, kTexture);
                            }

                            ktxTexture2_Destroy(kTexture);
                        }
                    }
                    break;
                    default:
                    {
                        unsigned char* data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                                                                    static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
                        if (data) {
                            VkExtent3D imagesize;
                            imagesize.width = width;
                            imagesize.height = height;
                            imagesize.depth = 1;
                            const size_t size = width * height * 4;
                            newImage = resourceManager.createImageFromData(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                                           VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                           true);

                            stbi_image_free(data);
                        }
                        else {
                            fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                        }
                    }
                    break;
                }
            },
            [&](const fastgltf::sources::BufferView& view) {
                const fastgltf::BufferView& bufferView = asset.bufferViews[view.bufferViewIndex];
                const fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];
                // We only care about VectorWithMime here, because we
                // specify LoadExternalBuffers, meaning all buffers
                // are already loaded into a vector.
                std::visit(fastgltf::visitor{
                               [](auto&) {},
                               [&](const fastgltf::sources::Array& vector) {
                                   const int32_t ktxVersion = isKtxTexture(vector);
                                   switch (ktxVersion) {
                                       case 1:
                                       {
                                           if (validateVector(vector, 0, vector.bytes.size())) {
                                               ktxTexture1* kTexture;
                                               const KTX_error_code ktxResult = ktxTexture1_CreateFromMemory(
                                                   reinterpret_cast<const unsigned char*>(vector.bytes.data() + bufferView.byteOffset),
                                                   bufferView.byteLength,
                                                   KTX_TEXTURE_CREATE_NO_FLAGS,
                                                   &kTexture);


                                               if (ktxResult == KTX_SUCCESS) {
                                                   newImage = processKtxVector(resourceManager, kTexture);
                                               }

                                               ktxTexture1_Destroy(kTexture);
                                           }
                                           else {
                                               fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                                           }
                                       }
                                       break;
                                       case 2:
                                       {
                                           if (validateVector(vector, 0, vector.bytes.size())) {
                                               ktxTexture2* kTexture;

                                               const KTX_error_code ktxResult = ktxTexture2_CreateFromMemory(
                                                   reinterpret_cast<const unsigned char*>(vector.bytes.data() + bufferView.byteOffset),
                                                   bufferView.byteLength,
                                                   KTX_TEXTURE_CREATE_NO_FLAGS,
                                                   &kTexture);

                                               if (ktxResult == KTX_SUCCESS) {
                                                   newImage = processKtxVector(resourceManager, kTexture);
                                               }

                                               ktxTexture2_Destroy(kTexture);
                                           }
                                           else {
                                               fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                                           }
                                       }
                                       break;
                                       default:
                                           fmt::print("Error: Failed to get correct ktx version from buffer view\n");
                                           break;
                                   }
                               }
                           }, buffer.data);
            }
        }, image.data);


    if (!newImage) {
        fmt::print("Image failed to load: {}\n", image.name.c_str());
        return {};
    }

    return newImage;
}

MaterialProperties model_utils::extractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial)
{
    MaterialProperties material = {};
    material.colorFactor = glm::vec4(
        gltfMaterial.pbrData.baseColorFactor[0],
        gltfMaterial.pbrData.baseColorFactor[1],
        gltfMaterial.pbrData.baseColorFactor[2],
        gltfMaterial.pbrData.baseColorFactor[3]);

    material.metalRoughFactors.x = gltfMaterial.pbrData.metallicFactor;
    material.metalRoughFactors.y = gltfMaterial.pbrData.roughnessFactor;

    material.alphaProperties.x = gltfMaterial.alphaCutoff;
    material.alphaProperties.z = gltfMaterial.doubleSided ? 1.0f : 0.0f;
    material.alphaProperties.w = gltfMaterial.unlit ? 1.0f : 0.0f;

    switch (gltfMaterial.alphaMode) {
        case fastgltf::AlphaMode::Opaque:
            material.alphaProperties.y = static_cast<float>(MaterialType::OPAQUE);
            break;
        case fastgltf::AlphaMode::Blend:
            material.alphaProperties.y = static_cast<float>(MaterialType::TRANSPARENT);
            break;
        case fastgltf::AlphaMode::Mask:
            material.alphaProperties.y = static_cast<float>(MaterialType::MASK);
            break;
    }

    material.emissiveFactor = glm::vec4(
        gltfMaterial.emissiveFactor[0],
        gltfMaterial.emissiveFactor[1],
        gltfMaterial.emissiveFactor[2],
        gltfMaterial.emissiveStrength);

    material.physicalProperties.x = gltfMaterial.ior;
    material.physicalProperties.y = gltfMaterial.dispersion;

    if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
        loadTextureIndicesAndUV(gltfMaterial.pbrData.baseColorTexture.value(), gltf,
                                material.textureImageIndices.x, material.textureSamplerIndices.x,
                                material.colorUvTransform);
    }


    if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value()) {
        loadTextureIndicesAndUV(gltfMaterial.pbrData.metallicRoughnessTexture.value(), gltf,
                                material.textureImageIndices.y, material.textureSamplerIndices.y,
                                material.metalRoughUvTransform);
    }


    if (gltfMaterial.normalTexture.has_value()) {
        loadTextureIndicesAndUV(gltfMaterial.normalTexture.value(), gltf,
                                material.textureImageIndices.z, material.textureSamplerIndices.z,
                                material.normalUvTransform);
        material.physicalProperties.z = gltfMaterial.normalTexture->scale;
    }

    if (gltfMaterial.emissiveTexture.has_value()) {
        loadTextureIndicesAndUV(gltfMaterial.emissiveTexture.value(), gltf,
                                material.textureImageIndices.w, material.textureSamplerIndices.w,
                                material.emissiveUvTransform);
    }

    if (gltfMaterial.occlusionTexture.has_value()) {
        loadTextureIndicesAndUV(gltfMaterial.occlusionTexture.value(), gltf,
                                material.textureImageIndices2.x, material.textureSamplerIndices2.x,
                                material.occlusionUvTransform);
        material.physicalProperties.w = gltfMaterial.occlusionTexture->strength;
    }

    if (gltfMaterial.packedNormalMetallicRoughnessTexture.has_value()) {
        fmt::print("Error: renderer does not support packed normal metallic roughness texture");
    }

    // Handle edge cases for missing samplers/images
    auto fixTextureIndices = [](int& imageIdx, int& samplerIdx) {
        if (imageIdx == -1 && samplerIdx != -1) imageIdx = 0;
        if (samplerIdx == -1 && imageIdx != -1) samplerIdx = 0;
    };

    fixTextureIndices(material.textureImageIndices.x, material.textureSamplerIndices.x);
    fixTextureIndices(material.textureImageIndices.y, material.textureSamplerIndices.y);
    fixTextureIndices(material.textureImageIndices.z, material.textureSamplerIndices.z);
    fixTextureIndices(material.textureImageIndices.w, material.textureSamplerIndices.w);
    fixTextureIndices(material.textureImageIndices2.x, material.textureSamplerIndices2.x);
    fixTextureIndices(material.textureImageIndices2.y, material.textureSamplerIndices2.y);

    return material;
}

void model_utils::loadTextureIndicesAndUV(const fastgltf::TextureInfo& texture, const fastgltf::Asset& gltf,
                                          int& imageIndex, int& samplerIndex, glm::vec4& uvTransform)
{
    const size_t textureIndex = texture.textureIndex;

    if (gltf.textures[textureIndex].basisuImageIndex.has_value()) {
        imageIndex = gltf.textures[textureIndex].basisuImageIndex.value() + imageOffset;
    }
    else if (gltf.textures[textureIndex].imageIndex.has_value()) {
        imageIndex = gltf.textures[textureIndex].imageIndex.value() + imageOffset;
    }

    if (gltf.textures[textureIndex].samplerIndex.has_value()) {
        samplerIndex = gltf.textures[textureIndex].samplerIndex.value() + samplerOffset;
    }

    if (texture.transform) {
        const auto& transform = texture.transform;
        uvTransform.x = transform->uvScale[0];
        uvTransform.y = transform->uvScale[1];
        uvTransform.z = transform->uvOffset[0];
        uvTransform.w = transform->uvOffset[1];
    }
}

VkFilter model_utils::extractFilter(fastgltf::Filter filter)
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

VkSamplerMipmapMode model_utils::extractMipMapMode(fastgltf::Filter filter)
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

int32_t model_utils::isKtxTexture(const fastgltf::sources::Array& vector)
{
    if (vector.bytes.size() >= 12) {
        // KTX1 identifier starts with 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB
        static constexpr unsigned char ktxIdentifier[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB};
        if (memcmp(vector.bytes.data(), ktxIdentifier, 8) == 0) {
            return 1;
        }

        // See ktxspec.v2 Section 3.1
        // KTX2 identifier starts with 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB
        static constexpr unsigned char ktx2Identifier[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB};
        if (memcmp(vector.bytes.data(), ktx2Identifier, 8) == 0) {
            return 2;
        }
    }

    return 0;
}

bool model_utils::validateVector(const fastgltf::sources::Array& vector, size_t offset, const size_t length)
{
    // Validation checks
    if (vector.bytes.empty()) {
        fmt::print("Error: Empty vector bytes\n");
        return false;
    }

    if (offset >= vector.bytes.size()) {
        fmt::print("Error: Offset ({}) >= vector size ({})\n", offset, vector.bytes.size());
        return false;
    }

    if (offset + length > vector.bytes.size()) {
        fmt::print("Error: Offset + length ({}) > vector size ({})\n", offset + length, vector.bytes.size());
        return false;
    }

    if (length == 0) {
        fmt::print("Error: Length is zero\n");
        return false;
    }

    return true;
}

ImageResourcePtr model_utils::processKtxVector(ResourceManager& resourceManager, ktxTexture1* kTexture)
{
    const VkFormat format{ktxTexture1_GetVkFormat(kTexture)};
    const ImageFormatProperties formatProperties = resourceManager.getPhysicalDeviceImageFormatProperties(format, VK_IMAGE_USAGE_SAMPLED_BIT);

    if (formatProperties.result != VK_SUCCESS) {
        fmt::print("Error: Format property failed check (usually means not supported).\n");
        return {};
    }

    std::unique_ptr<ImageKtx> newImage = resourceManager.createResource<ImageKtx>(ktxTexture(kTexture));
    return newImage;
}

ImageResourcePtr model_utils::processKtxVector(ResourceManager& resourceManager, ktxTexture2* kTexture)
{
    VkFormat format{VK_FORMAT_UNDEFINED};
    if (ktxTexture2_NeedsTranscoding(kTexture)) {
        // todo: choose appropriate format?
        constexpr auto targetFormat = KTX_TTF_BC7_RGBA;
        //constexpr ktx_transcode_fmt_e targetFormat = KTX_TTF_RGBA32;

        const KTX_error_code result = ktxTexture2_TranscodeBasis(kTexture, targetFormat, 0);
        if (result != KTX_SUCCESS) {
            fmt::print("Error: Ktx texture transcoding error.\n");
            return {};
        }

        format = ktxTexture2_GetVkFormat(kTexture);
    }
    else {
        format = ktxTexture2_GetVkFormat(kTexture);
    }

    const ImageFormatProperties formatProperties = resourceManager.getPhysicalDeviceImageFormatProperties(format, VK_IMAGE_USAGE_SAMPLED_BIT);


    if (formatProperties.result != VK_SUCCESS) {
        fmt::print("Error: Format property failed check (usually means not supported).\n");
        return {};
    }

    std::unique_ptr<ImageKtx> newImage = resourceManager.createResource<ImageKtx>(ktxTexture(kTexture));
    return newImage;
}
}
