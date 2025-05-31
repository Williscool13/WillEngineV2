//
// Created by William on 2025-04-14.
//

#ifndef CONTACT_SHADOWS_H
#define CONTACT_SHADOWS_H

#include <vulkan/vulkan_core.h>

#include "contact_shadows_pipeline_types.h"
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
    explicit ContactShadowsPipeline(ResourceManager& resourceManager);

    ~ContactShadowsPipeline();

    void setupDescriptorBuffer(const VkImageView& depthImageView);

    void draw(VkCommandBuffer cmd, const ContactShadowsDrawInfo& drawInfo);

    void reloadShaders()
    {
        createPipeline();
    }

    VkImageView getContactShadowRenderTarget() const { return contactShadowImage->imageView; }

private:
    void createPipeline();

    static DispatchList buildDispatchList(const Camera* camera, const DirectionalLight& mainLight);

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
