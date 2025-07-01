//
// Created by William on 2025-05-30.
//

#ifndef RESOURCES_FWD_H
#define RESOURCES_FWD_H
#include <memory>


namespace will_engine::renderer
{
struct RenderTarget;
struct DescriptorSetLayout;
struct Buffer;
struct DescriptorBufferUniform;
struct DescriptorBufferSampler;
struct ImageView;
struct Image;
struct ImageKtx;
struct ImageResource;
struct Sampler;
struct Pipeline;
struct PipelineLayout;
struct ShaderModule;


using BufferPtr = std::unique_ptr<Buffer>;
using DescriptorBufferSamplerPtr = std::unique_ptr<DescriptorBufferSampler>;
using DescriptorBufferUniformPtr = std::unique_ptr<DescriptorBufferUniform>;
using ImageViewPtr = std::unique_ptr<ImageView>;
using ImagePtr = std::unique_ptr<Image>;
using ImageKtxPtr = std::unique_ptr<ImageKtx>;
using ImageResourcePtr = std::unique_ptr<ImageResource>;
using RenderTargetPtr = std::unique_ptr<RenderTarget>;
using DescriptorSetLayoutPtr = std::unique_ptr<DescriptorSetLayout>;
using SamplerPtr = std::unique_ptr<Sampler>;
using PipelineLayoutPtr = std::unique_ptr<PipelineLayout>;
using PipelinePtr = std::unique_ptr<Pipeline>;
using ShaderModulePtr = std::unique_ptr<ShaderModule>;
}

#endif //RESOURCES_FWD_H
