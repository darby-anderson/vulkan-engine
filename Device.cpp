//
// Created by darby on 11/24/2024.
//

#include "Device.hpp"

void Device::reserve_queues(VkSurfaceKHR surface) {
    uint32_t queue_family_count {0};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_family_properties.data());

    int i = 0;
    for(const auto& queue_family : queue_family_properties) {
        VkBool32 present_support;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);

        if(!family_index_presentation && present_support) {
            family_index_presentation = i;
        }

        if(!family_index_graphics.has_value() && (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            family_index_graphics = i;
        }

        i++;

        if(family_index_graphics.has_value() && family_index_presentation.has_value()) {
            break;
        }
    }

}

void Device::init(VkPhysicalDevice physical_device_, VkSurfaceKHR surface) {

    this->physical_device = physical_device_;

    reserve_queues(surface);
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family_index_graphics.value(),
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    // Vulkan 1.3 Features
    VkPhysicalDeviceVulkan13Features vulkan13Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.synchronization2 = VK_TRUE;

    // Vulkan 1.2 Features
    VkPhysicalDeviceVulkan12Features vulkan12Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &vulkan13Features
    };
    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceFeatures2 physical_device_features_2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &vulkan12Features
    };

    const std::vector<const char*> required_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo device_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &physical_device_features_2,
            .queueCreateInfoCount = 1u,
            .pQueueCreateInfos = &queue_create_info,
            .enabledExtensionCount = 1u,
            .ppEnabledExtensionNames = required_extensions.data()
    };

    VK_CHECK(vkCreateDevice(physical_device, &device_create_info, nullptr, &device));

    vkGetDeviceQueue(device, family_index_graphics.value(), 0, &graphics_queue);
    vkGetDeviceQueue(device, family_index_presentation.value(), 0, &presentation_queue);

}


void Device::cleanup() {
    vkDestroyDevice(device, nullptr);
}
