//
// Created by darby on 12/1/2024.
//

#pragma once

#include <vector>
#include "volk.h"

#include "Window.hpp"

class Swapchain {

public:
    void init(VkDevice device,
            VkSurfaceKHR surface,
            VkSurfaceCapabilitiesKHR surface_capabilities,
            const std::vector<VkSurfaceFormatKHR>& available_surface_formats,
            const std::vector<VkPresentModeKHR>& available_present_modes,
            const Window& window);

    void cleanup();

    uint32_t get_current_swapchain_image_index(VkSemaphore swapchain_image_ready_semaphore, bool& resize_requested);
    VkImage get_swapchain_image(uint32_t index);
    VkImageView get_swapchain_image_view(uint32_t index);

    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;

    VkSwapchainKHR swapchain;

    uint32_t image_count;

private:
    VkDevice device;
    std::vector<VkImage> images; // OS-owned images associated with this swapchain
    std::vector<VkImageView> image_views;

};

