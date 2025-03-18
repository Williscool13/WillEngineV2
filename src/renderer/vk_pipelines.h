//
// Created by William on 2024-08-18.
//

#ifndef VK_PIPELINES_H
#define VK_PIPELINES_H

#include <vector>

#include <vulkan/vulkan_core.h>

#include "vk_helpers.h"

namespace will_engine
{
class PipelineBuilder
{
public:
    enum class BlendMode
    {
        ALPHA_BLEND,
        ADDITIVE_BLEND,
        NO_BLEND
    };

    PipelineBuilder() { clear(); }

    void clear();

    VkPipeline buildPipeline(VkDevice device, VkPipelineCreateFlagBits flags, std::vector<VkDynamicState> additionalDynamicStates = {});

    void setShaders(VkShaderModule vertexShader);

    void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    void setShaders(VkShaderModule vertexShader, VkShaderModule tessControlShader, VkShaderModule tessEvalShader, VkShaderModule fragmentShader);

    /**
     * Care must be taken when using this to ensure that the pointers are still valid when the pipeline is constructed
     * @param bindings
     * @param bindingCount
     * @param attributes
     * @param attributeCount
     */
    void setupVertexInput(const VkVertexInputBindingDescription* bindings, uint32_t bindingCount, const VkVertexInputAttributeDescription* attributes, uint32_t attributeCount);

    void setupInputAssembly(VkPrimitiveTopology topology, bool enablePrimitiveRestart = false);

    void setupRasterization(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace, bool rasterizerDiscardEnable = false);

    /**
     * Use to initialize the depth bias of the pipeline.
     * \n Additionally dynamically set depth b ias with \code vkCmdSetDepthBias\endcode
     * @param depthBiasConstantFactor
     * @param depthBiasClamp
     * @param depthBiasSlopeFactor
     */
    void enableDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);

    void setupMultisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples, float minSampleShading, const VkSampleMask* pSampleMask,
                            VkBool32 alphaToCoverageEnable, VkBool32 alphaToOneEnable);

    void setupRenderer(const std::vector<VkFormat>& colorattachmentFormat, VkFormat depthAttachmentFormat);

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
    void setupDepthStencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp
                           , VkBool32 depthBoundsTestEnable, VkBool32 stencilTestEnable, VkStencilOpState front, VkStencilOpState back
                           , float minDepthBounds, float maxDepthBounds);


    void setupBlending(PipelineBuilder::BlendMode mode);

    void setupPipelineLayout(VkPipelineLayout pipelineLayout);

    /**
     * Shortcut to disable multisampling for this pipeline
     */
    void disableMultisampling();

    /**
     * Shortcut to enable depth testing
     * @param depthWriteEnable enable depth write
     * @param op operation to use
     */
    void enableDepthTest(bool depthWriteEnable, VkCompareOp op);

    /**
     * Shortcut to disable depth testing for this pipeline
     */
    void disableDepthTest();

    VkPipelineDynamicStateCreateInfo generateDynamicStates(VkDynamicState states[], uint32_t count);

    void setupTessellation(int32_t controlPoints = 4);

private:
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    bool vertexInputEnabled{false};
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineMultisampleStateCreateInfo multisampling{};
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    VkPipelineRenderingCreateInfo renderInfo{};
    VkPipelineDepthStencilStateCreateInfo depthStencil{};

    bool bIsTessellationEnabled{false};
    VkPipelineTessellationStateCreateInfo tessellation{};


    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};

    // keep to avoid dangling pointer for color attachment (referenced by pointer)
    std::vector<VkFormat> colorAttachmentFormats;
};
}



#endif //VK_PIPELINES_H
