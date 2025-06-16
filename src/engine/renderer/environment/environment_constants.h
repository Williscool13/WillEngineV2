//
// Created by William on 2025-01-25.
//

#ifndef ENVIRONMENT_CONSTANTS_H
#define ENVIRONMENT_CONSTANTS_H
#include <vulkan/vulkan_core.h>

namespace will_engine::renderer
{
constexpr float DIFFUSE_SAMPLE_DELTA{0.025f};
constexpr int32_t SPECULAR_SAMPLE_COUNT{2048};
constexpr uint32_t CUBEMAP_RESOLUTION{1024}; // 1024 == .4k HDR file
constexpr VkExtent3D CUBEMAP_EXTENTS{CUBEMAP_RESOLUTION, CUBEMAP_RESOLUTION, 1};
constexpr int32_t MAX_ENVIRONMENT_MAPS{10};
constexpr int32_t ENVIRONMENT_MAP_MIP_COUNT{6};
constexpr int32_t SPECULAR_PREFILTERED_MIP_LEVELS{ENVIRONMENT_MAP_MIP_COUNT - 1};
constexpr VkExtent3D SPECULAR_PREFILTERED_BASE_EXTENTS{512, 512, 1};
constexpr VkExtent3D LUT_IMAGE_EXTENT{512, 512, 1};
}

#endif //ENVIRONMENT_CONSTANTS_H
