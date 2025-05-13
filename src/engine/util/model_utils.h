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


namespace will_engine::model_utils
{
static constexpr int32_t samplerOffset{1};
static constexpr int32_t imageOffset{1};

[[nodiscard]] std::optional<AllocatedImage> loadImage(const ResourceManager& resourceManager, const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& parentFolder);

MaterialProperties extractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial);

void loadTextureIndices(const fastgltf::Optional<fastgltf::TextureInfo>& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex);

VkFilter extractFilter(fastgltf::Filter filter);

VkSamplerMipmapMode extractMipMapMode(fastgltf::Filter filter);
}


#endif //MODEL_UTILS_H
