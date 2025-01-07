//
// Created by darby on 12/1/2024.
//

#include <limits>
#include <algorithm>
#include <limits>

#include "Common.hpp"
#include "Swapchain.hpp"
#include "VulkanInitUtility.hpp"

#include "fmt/base.h"
#include "GLFW/glfw3.h"


void Swapchain::init(VkDevice device,
                    VkSurfaceKHR surface,
                    const VkSurfaceCapabilitiesKHR surface_capabilities,
                    const std::vector<VkSurfaceFormatKHR>& available_surface_formats,
                    const std::vector<VkPresentModeKHR>& available_present_modes,
                    const Window& window) {

    this->device = device;

    // Choose surface format
    bool surface_format_found = false;
    for(const auto& available_format : available_surface_formats) {
        if(available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format_found = true;
            surface_format = available_format;
            break;
        }
    }

    if(!surface_format_found) {
        fmt::print("Desired surface format not available. Choosing first available format.\n");
        surface_format = available_surface_formats[0];
    }

    // Choose present mode
    bool present_mode_found = false;
    for(const auto& available_present_mode : available_present_modes) {
        if(available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode_found = true;
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    if(!present_mode_found) {
        fmt::print("Desired present mode not available. Choosing FIFO Mode\n");
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
    }

    int width, height;
    window.get_window_framebuffer_size(width, height);

    fmt::print("window width, height {}, {}\n", width, height);
    fmt::print("min image extent width, height {}, {}\n", surface_capabilities.minImageExtent.width, surface_capabilities.minImageExtent.height);
    fmt::print("max image extent width, height {}, {}\n", surface_capabilities.maxImageExtent.width, surface_capabilities.maxImageExtent.height);

    extent.width = static_cast<uint32_t>(width);
    extent.height = static_cast<uint32_t>(height);

    int monitor_w, monitor_h;
    window.get_primary_monitor_resolution(monitor_w, monitor_h);

    extent.width = std::clamp(extent.width, 100u, static_cast<uint32_t>(monitor_w));
    extent.height = std::clamp(extent.height, 100u, static_cast<uint32_t>(monitor_h));

    fmt::print("Swapchain Extent: ({}, {})\n", extent.width, extent.height);

    // Get swapchain image count
    image_count = surface_capabilities.minImageCount + 1;

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    VK_CHECK(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain));

    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr));
    images.resize(image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images.data()));

    // Create views of swapchain images
    image_views.resize(image_count);
    for(int i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo view_create_info = vk_init::get_image_view_create_info(surface_format.format, images[i], VK_IMAGE_ASPECT_COLOR_BIT);

        VK_CHECK(vkCreateImageView(device, &view_create_info, nullptr, &image_views[i]));
    }
}

void Swapchain::cleanup() {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

VkImage Swapchain::get_swapchain_image(uint32_t index) {
    return images[index];
}

VkImageView Swapchain::get_swapchain_image_view(uint32_t index) {
    return image_views[index];
}

uint32_t Swapchain::get_current_swapchain_image_index(VkSemaphore swapchain_image_ready_semaphore, bool& resize_requested) {
    uint32_t swapchain_image_index;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, 1000000000, swapchain_image_ready_semaphore, nullptr, &swapchain_image_index);

    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        fmt::print("error out of date khr\n");
        resize_requested = true;
    } else if(result != VK_SUCCESS) {
        ASSERT(true, "vkAcquireNextImage failing\n");
    } else { // SUCCESS
        resize_requested = false;
    }

    return swapchain_image_index;
}
