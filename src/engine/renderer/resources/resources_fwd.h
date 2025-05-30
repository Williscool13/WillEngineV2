//
// Created by William on 2025-05-30.
//

#ifndef RESOURCES_FWD_H
#define RESOURCES_FWD_H
#include <memory>

namespace will_engine::renderer
{
struct AllocatedBuffer;
struct DescriptorBufferUniform;
struct DescriptorBufferSampler;
struct ImageWithView;
struct ImageView;
struct Image;
struct ImageKtx;
struct Sampler;
struct DescriptorSetLayout;
struct Pipeline;
struct PipelineLayout;


using AllocatedBufferPtr = std::unique_ptr<AllocatedBuffer>;
using DescriptorBufferSamplerPtr = std::unique_ptr<DescriptorBufferSampler>;
using DescriptorBufferUniformPtr = std::unique_ptr<DescriptorBufferUniform>;
using ImageWithViewPtr = std::unique_ptr<ImageWithView>;
using ImageViewPtr = std::unique_ptr<ImageView>;
using ImagePtr = std::unique_ptr<Image>;
using ImageKtxPtr = std::unique_ptr<ImageKtx>;
using DescriptorSetLayoutPtr = std::unique_ptr<DescriptorSetLayout>;
using SamplerPtr = std::unique_ptr<Sampler>;
using PipelineLayoutPtr = std::unique_ptr<PipelineLayout>;
using PipelinePtr = std::unique_ptr<Pipeline>;
}

#endif //RESOURCES_FWD_H
