//
// Created by William on 2024-08-18.
//

#ifndef VK_PIPELINES_H
#define VK_PIPELINES_H

#include <vector>

#include <vulkan/vulkan_core.h>
#include <fmt/format.h>

#include "../util/vk_helpers.h"

class PipelineBuilder {
public:
    enum class BlendMode {
        ALPHA_BLEND,
        ADDITIVE_BLEND,
        NO_BLEND
    };

    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

    VkPipelineInputAssemblyStateCreateInfo _inputAssembly{};
    VkPipelineRasterizationStateCreateInfo _rasterizer{};
    VkPipelineMultisampleStateCreateInfo _multisampling{};
    VkPipelineColorBlendAttachmentState _colorBlendAttachment{};
    VkPipelineRenderingCreateInfo _renderInfo{};
    VkPipelineDepthStencilStateCreateInfo _depthStencil{};

    VkPipelineLayout _pipelineLayout{VK_NULL_HANDLE};
    VkFormat _colorAttachmentFormat{};

    PipelineBuilder() { clear(); }

    void clear();

    VkPipeline build_pipeline(VkDevice device, VkPipelineCreateFlagBits flags);

    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    void setup_input_assembly(VkPrimitiveTopology topology);

    void setup_rasterization(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);

    void setup_multisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples
                             , float minSampleShading, const VkSampleMask *pSampleMask
                             , VkBool32 alphaToCoverageEnable, VkBool32 alphaToOneEnable);

    void setup_renderer(VkFormat colorattachmentFormat, VkFormat depthAttachmentFormat);

    /**
     * Set up the depth and stencil for this pipeline
     * @param depthTestEnable enable depth test
     * @param depthWriteEnable enable depth write
     * @param compareOp compare operationb to use
     * @param depthBoundsTestEnable
     * @param stencilTestEnable
     * @param front
     * @param back
     * @param minDepthBounds
     * @param maxDepthBounds
     */
    void setup_depth_stencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp
                             , VkBool32 depthBoundsTestEnable, VkBool32 stencilTestEnable, VkStencilOpState front, VkStencilOpState back
                             , float minDepthBounds, float maxDepthBounds);


    void setup_blending(PipelineBuilder::BlendMode mode);


    /**
     * Shortcut to disable multisampling for this pipeline
     */
    void disable_multisampling();

    /**
     * Shortcut to enable depth testing
     * @param depthWriteEnable enable depth write
     * @param op operation to use
     */
    void enable_depthtest(bool depthWriteEnable, VkCompareOp op);

    /**
     * Shortcut to disable depth testing for this pipeline
     */
    void disable_depthtest();

    VkPipelineDynamicStateCreateInfo generateDynamicStates(VkDynamicState states[], uint32_t count);
};


#endif //VK_PIPELINES_H