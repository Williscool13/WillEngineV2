//
// Created by William on 2025-04-14.
//

#ifndef POST_PROCESS_PIPELINE_TYPES_H
#define POST_PROCESS_PIPELINE_TYPES_H

#include <volk/volk.h>

namespace will_engine::post_process_pipeline
{
enum class PostProcessType : uint32_t
{
    None = 0x00000000,
    Tonemapping = 1 << 0,
    Sharpening = 1 << 1,
    FXAA = 1 << 2,
    ALL = 0xFFFFFFFF
};

inline PostProcessType operator|(PostProcessType a, PostProcessType b)
{
    return static_cast<PostProcessType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline PostProcessType operator&(PostProcessType a, PostProcessType b)
{
    return static_cast<PostProcessType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline PostProcessType operator~(PostProcessType a)
{
    return static_cast<PostProcessType>(~static_cast<uint32_t>(a));
}

inline PostProcessType& operator|=(PostProcessType& a, PostProcessType b)
{
    return a = a | b;
}

inline PostProcessType& operator&=(PostProcessType& a, PostProcessType b)
{
    return a = a & b;
}


struct PostProcessDescriptor
{
    VkImageView inputImage;
    VkImageView outputImage;
    VkSampler sampler;
};

struct PostProcessPushConstants
{
    int32_t width;
    int32_t height;
    uint32_t postProcessFlags;
    int32_t padding;
};

struct PostProcessDrawInfo
{
    PostProcessType postProcessFlags{PostProcessType::ALL};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};
}

#endif //POST_PROCESS_PIPELINE_TYPES_H
