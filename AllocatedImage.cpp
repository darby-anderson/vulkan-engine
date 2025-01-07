//
// Created by darby on 1/6/2025.
//

#include "AllocatedImage.hpp"
#include "Buffer.hpp"
#include "VulkanInitUtility.hpp"
#include "VulkanImageUtility.hpp"

void AllocatedImage::init(VkDevice device, VmaAllocator _allocator, VkExtent3D size, VkFormat _format, VkImageUsageFlags usage_flags, bool mipmapped) {
    this->extent = size;
    this->format = _format;
    this->allocator = _allocator;

    VkImageCreateInfo image_create_info = vk_init::get_image_create_info(this->format, usage_flags, this->extent);
    if(mipmapped) {
        image_create_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo image_alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vmaCreateImage(allocator, &image_create_info, &image_alloc_info, &this->image, &this->allocation, nullptr));

    VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    if(format == VK_FORMAT_D32_SFLOAT) {
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo image_view_create_info = vk_init::get_image_view_create_info(this->format, this->image, aspect_flags);
    image_view_create_info.subresourceRange.levelCount = image_create_info.mipLevels;
    VK_CHECK(vkCreateImageView(device, &image_view_create_info, nullptr, &this->view));
}


void AllocatedImage::init_with_data(ImmediateSubmitCommandBuffer& immediate_submit_command_buffer, VkDevice device, VmaAllocator _allocator,
                                    void* data, VkExtent3D size, VkFormat _format, VkImageUsageFlags usage_flags, bool mipmapped) {

    size_t data_size = size.depth * size.width * size.height * 4; // four channels
    Buffer upload_buffer{};
    upload_buffer.init(_allocator, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(upload_buffer.info.pMappedData, data, data_size);

    this->init(device, allocator, size, _format, usage_flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
               mipmapped);

    immediate_submit_command_buffer.submit([&](VkCommandBuffer cmd) {
        vk_image::transition_image_layout(cmd, this->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copy_region = {};
        copy_region.bufferOffset = 0;
        copy_region.bufferRowLength = 0;
        copy_region.bufferImageHeight = 0;

        copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.mipLevel = 0;
        copy_region.imageSubresource.baseArrayLayer = 0;
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageExtent = size;

        vkCmdCopyBufferToImage(cmd, upload_buffer.buffer, this->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

        vk_image::transition_image_layout(cmd, this->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    upload_buffer.destroy_buffer();
}

void AllocatedImage::destroy(VkDevice device) {
    vkDestroyImageView(device, view, nullptr);
    vmaDestroyImage(allocator, image, allocation);
}

