//
// Created by darby on 11/24/2024.
//

#pragma once

#include "Common.hpp"

class VulkanContext {

public:
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    void init_vulkan_instance();
    void init_vulkan_surface(void* window_handle);

};
