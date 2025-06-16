//
// Created by William on 2025-02-22.
//

#ifndef MODEL_UTILS_H
#define MODEL_UTILS_H
#include <optional>
#include <fastgltf/types.hpp>

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_types.h"
#include "engine/renderer/assets/render_object/render_object_types.h"


namespace will_engine::renderer::model_utils
{
static constexpr int32_t samplerOffset{1};
static constexpr int32_t imageOffset{1};

/**
 * As specified by the GLTF 2.0 - Section 3.9.2., the base color texture must contain 8-bit values encoded with sRGB.
 * This means that the output VkFormat should be an SRGB format and will be hardware decoded in the shaders
 * https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
 * @param resourceManager
 * @param asset
 * @param image
 * @param parentFolder
 * @return
 */
[[nodiscard]] ImageResourcePtr loadImage(ResourceManager& resourceManager, const fastgltf::Asset& asset, const fastgltf::Image& image,
                                         const std::filesystem::path& parentFolder);

MaterialProperties extractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial);

void loadTextureIndices(const fastgltf::Optional<fastgltf::TextureInfo>& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex);

void loadTextureIndicesAndUV(const ::fastgltf::TextureInfo& texture, const ::fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex,
                             glm::vec4& uvTransform);

VkFilter extractFilter(fastgltf::Filter filter);

VkSamplerMipmapMode extractMipMapMode(fastgltf::Filter filter);

/**
 *
 * @param vector
 * @return 0 if not, number indicates ktx version
 */
static int32_t isKtxTexture(const fastgltf::sources::Array& vector);

static bool validateVector(const fastgltf::sources::Array& vector, size_t offset, size_t length);
static ImageResourcePtr processKtxVector(ResourceManager& resourceManager, ktxTexture1* kTexture);
static ImageResourcePtr processKtxVector(ResourceManager& resourceManager, ktxTexture2* kTexture);
}


#endif //MODEL_UTILS_H
