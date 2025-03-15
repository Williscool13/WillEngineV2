//
// Created by William on 2024-12-07.
//

#include "vulkan_context.h"

#include <SDL_vulkan.h>
#include <vk-bootstrap/VkBootstrap.h>
#include <volk/volk.h>

VulkanContext::VulkanContext(SDL_Window* window, bool useValidationLayers)
{
    if (VkResult res = volkInitialize(); res != VK_SUCCESS) {
        throw std::runtime_error("Failed to initialize volk");
    }

    vkb::InstanceBuilder builder;
    std::vector<const char*> enabledInstanceExtensions;
    enabledInstanceExtensions.push_back("VK_EXT_debug_utils");

    auto inst_ret = builder.set_app_name("Will's Vulkan Renderer")
            .request_validation_layers(useValidationLayers)
            .use_default_debug_messenger()
            .require_api_version(1, 3)
            .enable_extensions(enabledInstanceExtensions)
            .build();

    vkb::Instance vkb_inst = inst_ret.value();
    instance = vkb_inst.instance;
    volkLoadInstance(instance);
    debugMessenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(window, instance, nullptr,  &surface);

    // vk 1.3
    VkPhysicalDeviceVulkan13Features features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features.dynamicRendering = VK_TRUE;
    features.synchronization2 = VK_TRUE;

    // vk 1.2
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    VkPhysicalDeviceFeatures otherFeatures{};
    otherFeatures.multiDrawIndirect = VK_TRUE;
    otherFeatures.samplerAnisotropy = VK_TRUE;
    otherFeatures.tessellationShader = VK_TRUE;

    // Descriptor Buffer Extension
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {};
    descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descriptorBufferFeatures.descriptorBuffer = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice targetDevice = selector
            .set_minimum_version(1, 3)
            .set_required_features_13(features)
            .set_required_features_12(features12)
            .set_required_features(otherFeatures)
            .add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
            .set_surface(surface)
            .select()
            .value();

    vkb::DeviceBuilder deviceBuilder{targetDevice};
    deviceBuilder.add_pNext(&descriptorBufferFeatures);
    vkb::Device vkbDevice = deviceBuilder.build().value();

    device = vkbDevice.device;
    volkLoadDevice(device);
    physicalDevice = targetDevice.physical_device;

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

VulkanContext::~VulkanContext()
{
    if (allocator) {
        vmaDestroyAllocator(allocator);
    }
    if (surface) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    if (device) {
        vkDestroyDevice(device, nullptr);
    }
    if (debugMessenger) {
        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    }
    if (instance) {
        vkDestroyInstance(instance, nullptr);
    }
}
