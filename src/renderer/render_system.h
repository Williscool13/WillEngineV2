//
// Created by William on 2024-12-07.
//

#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H
#include <memory>
#include <vector>

#include "vulkan_context.h"
#include "render_object/render_object.h"


class RenderSystem {

public:
    explicit RenderSystem(VulkanContext& context);
    ~RenderSystem();

    void draw(VkCommandBuffer cmd);
    void drawEnvironment(VkCommandBuffer cmd) const;
    void drawDeferredMrt(VkCommandBuffer cmd) const;
    void drawDeferredResolve(VkCommandBuffer cmd) const;
    void drawTaa(VkCommandBuffer cmd) const;
    void drawPostProcess(VkCommandBuffer cmd) const;

    RenderObject* createRenderObject(const std::string_view& gltfPath);
    void destroyRenderObject(RenderObject* object);

    VulkanContext& getContext() { return vulkanContext; }

private:
    VulkanContext& vulkanContext;
    std::vector<std::unique_ptr<RenderObject>> renderObjects;

    // Pipeline layouts and other shared resources moved from Engine
    VkPipelineLayout deferredMrtPipelineLayout{VK_NULL_HANDLE};
    VkPipeline deferredMrtPipeline{VK_NULL_HANDLE};

    // ... other pipeline and resource declarations from Engine

    void initPipelines();
    void cleanup();

};


#endif //RENDER_SYSTEM_H
