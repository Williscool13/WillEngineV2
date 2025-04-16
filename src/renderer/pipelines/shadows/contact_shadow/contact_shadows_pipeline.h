//
// Created by William on 2025-04-14.
//

#ifndef CONTACT_SHADOWS_H
#define CONTACT_SHADOWS_H

#include <volk/volk.h>

#include "contact_shadows_pipeline_types.h"
#include "src/renderer/imgui_wrapper.h"
#include "src/renderer/vk_types.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"


namespace will_engine
{
class Camera;
class DirectionalLight;
}

namespace will_engine::contact_shadows_pipeline
{
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

    AllocatedImage getContactShadowRenderTarget() const { return contactShadowImage; }

private:
    void createPipeline();

    static static DispatchList buildDispatchList(const Camera* camera, const DirectionalLight& mainLight);

    static int32_t bend_min(const int32_t a, const int32_t b) { return a > b ? b : a; }
    static int32_t bend_max(const int32_t a, const int32_t b) { return a > b ? a : b; }

private:
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    VkSampler depthSampler{VK_NULL_HANDLE};

    VkFormat contactShadowFormat{VK_FORMAT_R8_UNORM};
    AllocatedImage contactShadowImage{VK_NULL_HANDLE};

    DescriptorBufferSampler descriptorBufferSampler;

private: // Debug
    VkFormat debugFormat{VK_FORMAT_R8G8B8A8_SINT};
    AllocatedImage debugImage{VK_NULL_HANDLE};

    ResourceManager& resourceManager;

private:
    //todo: remove
    friend void ImguiWrapper::imguiInterface(Engine* engine);
};

}

#endif //CONTACT_SHADOWS_H
