//
// Created by darby on 11/25/2024.
//

#include "PhysicalDevice.hpp"


std::vector<VkPhysicalDevice> queryPhysicalDevices(VkInstance instance) {

    uint32_t deviceCount {0};
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    return devices;
}

void PhysicalDevice::choose_and_init(VkInstance instance, VkSurfaceKHR surface) {

    // Vulkan 1.3 Features
    this->physical_device = queryPhysicalDevices(instance)[0];

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    VkPhysicalDeviceVulkan13Features query13Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };

    VkPhysicalDeviceVulkan12Features query12Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &query13Features
    };

    VkPhysicalDeviceFeatures2 features2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &query12Features
    };
    vkGetPhysicalDeviceFeatures2(physical_device, &features2);

    // Check the features we want are available
    if(!query13Features.dynamicRendering) {
        fmt::print("dynamicRendering not available on this device!");
    }

    if(!query13Features.synchronization2) {
        fmt::print("synchronization2 not available on this device!");
    }

    if(!query12Features.bufferDeviceAddress) {
        fmt::print("bufferDeviceAddress not available on this device!");
    }

    if(!query12Features.descriptorIndexing) {
        fmt::print("descriptorIndexing not available on this device!");
    }

    // Check the extensions we want are available
    uint32_t extension_count{0};
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> extension_properties(extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, extension_properties.data());

    std::vector<const char*> required_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    };

    fmt::print("--Found {} device extensions--\n", extension_properties.size());
    bool has_all_extensions = true;
    for(auto& extension_name : required_extensions) {
        bool found_extension = false;
        for(auto& property : extension_properties) {
            fmt::print("{}\n", property.extensionName);

            if(strcmp(property.extensionName, extension_name) == 0) {
                found_extension = true;
                break;
            }
        }

        if(!found_extension) {
            fmt::print("Physical device missing {} extension\n", extension_name);
            has_all_extensions = false;
        }
    }
    fmt::print("--End of device extension list--\n", extension_properties.size());

    // Check formats supported
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities));

    uint32_t format_count{0};
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr));
    surface_formats.resize(format_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, surface_formats.data()));

    uint32_t present_mode_count{0};
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr));
    present_modes.resize(present_mode_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data()));

    if(format_count == 0 || present_mode_count == 0) {
        fmt::print("ERROR! possibly no formats or present modes. format count {}, present mode count{}\n", format_count, present_mode_count);
    }

    VkPhysicalDeviceProperties2 device_properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };
    vkGetPhysicalDeviceProperties2(physical_device, &device_properties);

    fmt::print("Max color attachments: {}\n", device_properties.properties.limits.maxColorAttachments);



}
