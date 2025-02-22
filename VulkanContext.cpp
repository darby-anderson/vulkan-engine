//
// Created by darby on 11/24/2024.
//

#include "VulkanContext.hpp"

#define VK_USE_PLATFORM_WIN32_KHR
#include "volk.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h> // exposes GetModuleHandle for surface


std::vector<const char*> queryInstanceLayers() {
    uint32_t instanceLayerCount{0};
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));

    std::vector<VkLayerProperties> layers(instanceLayerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, layers.data()));

    fmt::print("--Found {} instance layers--\n", instanceLayerCount);
    // transfer from VkLayerProperties container into string container
    std::vector<const char*> availableLayers;
    for(auto & layer : layers) {
        availableLayers.emplace_back(layer.layerName);
        fmt::print("{}\n", layer.layerName);
    }
    fmt::print("--End of instance layer list--\n");

    return availableLayers;
}

std::vector<const char*> queryInstanceExtensions() {
    uint32_t instanceExtentionCount{0};
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtentionCount, nullptr));

    std::vector<VkExtensionProperties> extensions(instanceExtentionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtentionCount, extensions.data()));

    fmt::print("--Found {} instance extensions--\n", instanceExtentionCount);
    std::vector<const char*> availableExtensions;
    for(auto& extension : extensions) {
        availableExtensions.emplace_back(extension.extensionName);
        fmt::print("{}\n", extension.extensionName);
    }
    fmt::print("--End of instance extension list--\n", instanceExtentionCount);

    return availableExtensions;
}

VkBool32 VKAPI_PTR debugMessengerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {

    const char* severity = "";
    if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
        severity = "ERROR";
    } else if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)) {
        severity = "WARNING";
    } else if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)){
        severity = "INFO";
        return VK_FALSE;
    } else if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)) {
        severity = "VERBOSE";
        return VK_FALSE;
    }

    fmt::print(stderr, "debugMessengerCallback: |{}| Message Code: {}. Message: {}\n", severity, pCallbackData->pMessageIdName, pCallbackData->pMessage);

    return VK_FALSE;
}

void VulkanContext::init_vulkan_instance() {

    VK_CHECK(volkInitialize());

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Test Engine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    std::vector<const char*> availableInstanceLayers = queryInstanceLayers();
    std::vector<const char*> availableInstanceExtensions = queryInstanceExtensions();

    std::vector<const char*> requestedInstanceLayers = {
            "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> requestedInstanceExtensions = {
            "VK_KHR_win32_surface",
            "VK_KHR_surface",
            "VK_KHR_get_physical_device_properties2",
            "VK_EXT_debug_report",
            "VK_EXT_debug_utils"
    };

    VkInstanceCreateInfo instance_create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(requestedInstanceLayers.size()),
        .ppEnabledLayerNames = requestedInstanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requestedInstanceExtensions.size()),
        .ppEnabledExtensionNames = requestedInstanceExtensions.data()
    };

    VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &this->instance));
    volkLoadInstance(instance);

    // init debugging
    const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
            .pfnUserCallback = &debugMessengerCallback,
            .pUserData = nullptr,
    };
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(this->instance, &messengerInfo, nullptr, &this->debug_messenger));

}

void VulkanContext::init_vulkan_surface(void* window_handle) {
    if (window_handle != nullptr) {
        const VkWin32SurfaceCreateInfoKHR winCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hinstance = GetModuleHandle(NULL),
                .hwnd = (HWND) window_handle
        };

        VK_CHECK(vkCreateWin32SurfaceKHR(this->instance, &winCreateInfo, nullptr, &this->surface));
    }
}
