//
// Created by William on 2025-04-14.
//

#ifndef CONTACT_SHADOWS_H
#define CONTACT_SHADOWS_H

#include <volk/volk.h>

#include "contact_shadows_pipeline_types.h"
#include "engine/renderer/vk_types.h"
#include "engine/renderer/resources/allocated_image.h"
#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"


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

    VkImageView getContactShadowRenderTarget() const { return contactShadowImage.imageView; }

private:
    void createPipeline();

    static DispatchList buildDispatchList(const Camera* camera, const DirectionalLight& mainLight);

    static int32_t bend_min(const int32_t a, const int32_t b) { return a > b ? b : a; }
    static int32_t bend_max(const int32_t a, const int32_t b) { return a > b ? a : b; }

private:
    DescriptorSetLayout descriptorSetLayout{};
    PipelineLayout pipelineLayout{};
    Pipeline pipeline{};

    Sampler depthSampler{};

    VkFormat contactShadowFormat{VK_FORMAT_R8_UNORM};
    AllocatedImage contactShadowImage{};

    DescriptorBufferSampler descriptorBufferSampler;

private: // Debug
    VkFormat debugFormat{VK_FORMAT_R8G8B8A8_UNORM};
    AllocatedImage debugImage{};

    ResourceManager& resourceManager;
};

}

#endif //CONTACT_SHADOWS_H
