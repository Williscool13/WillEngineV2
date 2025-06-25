//
// Created by William on 2025-04-14.
//

#ifndef CONTACT_SHADOWS_H
#define CONTACT_SHADOWS_H

#include <vulkan/vulkan_core.h>

#include "contact_shadows_pipeline_types.h"
#include "engine/renderer/render_context.h"
#include "engine/renderer/resources/image_resource.h"
#include "engine/renderer/resources/resources_fwd.h"


namespace will_engine
{
class Camera;
struct DirectionalLight;
}

namespace will_engine::renderer
{
class ResourceManager;

class ContactShadowsPipeline {
public:
    explicit ContactShadowsPipeline(ResourceManager& resourceManager, RenderContext& renderContext);

    ~ContactShadowsPipeline();

    void setupDescriptorBuffer(const VkImageView& depthImageView) const;

    void draw(VkCommandBuffer cmd, const ContactShadowsDrawInfo& drawInfo);

    void reloadShaders()
    {
        createPipeline();
    }

    VkImageView getContactShadowRenderTarget() const { return contactShadowImage->imageView; }

private:
    void createPipeline();

    void createIntermediateRenderTargets(VkExtent2D extents);

    void handleResize(const ResolutionChangedEvent& event);

    EventDispatcher<ResolutionChangedEvent>::Handle resolutionChangedHandle;

    /**
     * Used to calculate dispatch lists
     */
    glm::vec2 cachedRenderExtents{DEFAULT_RENDER_EXTENT_2D.width, DEFAULT_RENDER_EXTENT_2D.height};

private:
    DispatchList buildDispatchList(const Camera* camera, const DirectionalLight& mainLight) const;

    static int32_t bend_min(const int32_t a, const int32_t b) { return a > b ? b : a; }
    static int32_t bend_max(const int32_t a, const int32_t b) { return a > b ? a : b; }

private:
    DescriptorSetLayoutPtr descriptorSetLayout{};
    PipelineLayoutPtr pipelineLayout{};
    PipelinePtr pipeline{};

    SamplerPtr depthSampler{};

    VkFormat contactShadowFormat{VK_FORMAT_R8_UNORM};
    ImageResourcePtr contactShadowImage{};

    DescriptorBufferSamplerPtr descriptorBufferSampler;

private: // Debug
    VkFormat debugFormat{VK_FORMAT_R8G8B8A8_UNORM};
    ImageResourcePtr debugImage{};

    ResourceManager& resourceManager;
};

}

#endif //CONTACT_SHADOWS_H
