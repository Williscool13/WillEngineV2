//
// Created by William on 2025-04-14.
//

#ifndef BASIC_COMPUTE_PIPELINE_TYPES_H
#define BASIC_COMPUTE_PIPELINE_TYPES_H

#include <volk/volk.h>

namespace will_engine::basic_compute_pipeline
{
struct ComputeDescriptorInfo
{
    VkImageView inputImage;
};

struct ComputeDrawInfo
{
    VkExtent2D renderExtent;
};
}

#endif //BASIC_COMPUTE_PIPELINE_TYPES_H
