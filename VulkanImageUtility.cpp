//
// Created by darby on 1/6/2025.
//

#include "VulkanImageUtility.hpp"
#include "VulkanInitUtility.hpp"

namespace vk_image {

// assumes image aspect we are interested in
void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout) {
    VkImageAspectFlags aspect_flags = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ?
                                      VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    transition_image_layout_specify_aspect(cmd, image, currentLayout, newLayout, aspect_flags);
}

void transition_image_layout_specify_aspect(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags) {

    VkImageMemoryBarrier2 imageBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT,
            .oldLayout = currentLayout,
            .newLayout = newLayout,
            .image = image,
            .subresourceRange = vk_init::get_image_subresource_range(aspectFlags),
    };

    VkDependencyInfo dep_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imageBarrier
    };

    vkCmdPipelineBarrier2(cmd, &dep_info);
}

void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D source_size, VkExtent2D destination_size) {
    VkImageSubresourceLayers src_layers = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
    };

    VkImageSubresourceLayers dst_layers = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
    };

    VkImageBlit2 blit_region = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr,
            .srcSubresource = src_layers,
            .dstSubresource = dst_layers
    };

    blit_region.srcOffsets[1] = {
            .x = static_cast<int32_t>(source_size.width),
            .y = static_cast<int32_t>(source_size.height),
            .z = 1
    };

    blit_region.dstOffsets[1] = {
            .x = static_cast<int32_t>(destination_size.width),
            .y = static_cast<int32_t>(destination_size.height),
            .z = 1
    };


    VkBlitImageInfo2 info = {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = source,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = destination,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blit_region,
            .filter = VK_FILTER_LINEAR,
    };

    vkCmdBlitImage2(cmd, &info);
}

}
