//
// Created by darby on 12/9/2024.
//

#pragma once

#include "Common.hpp"
#include "ImmediateSubmitCommandBuffer.hpp"

struct AllocatedImage {
    VkImage image;
    VkImageView view;
    VmaAllocator allocator;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;

    void init(VkDevice device, VmaAllocator allocator, VkExtent3D size, VkFormat format, VkImageUsageFlags usage_flags, bool mipmapped = false);
    void init_with_data(ImmediateSubmitCommandBuffer& immediate_submit_command_buffer, VkDevice device, VmaAllocator allocator, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage_flags, bool mipmapped = false);
    void destroy(VkDevice device);
};
