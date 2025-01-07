//
// Created by darby on 8/11/2024.
//

#pragma once

#include <string>
#include <vector>
#include "volk.h" // IMPORTANT! vk_mem_alloc.h includes the vulkan function, but not the impl.
#include "vk_mem_alloc.h" // a seg fault will be thrown if the implementation is not there.

namespace Fairy::Graphics {

class DynamicRendering final {

public:
    struct AttachmentDescription {
        VkImageView imageView;
        VkImageLayout imageLayout;
        VkResolveModeFlagBits resolveModeFlagBits = VK_RESOLVE_MODE_NONE;
        VkImageView resolveImageView = VK_NULL_HANDLE;
        VkImageLayout resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAttachmentLoadOp attachmentLoadOp;
        VkAttachmentStoreOp attachmentStoreOp;
        VkClearValue clearValue;
    };

    static std::string instanceExtensions();

    static void beginRenderingCmd(
        VkCommandBuffer commandBuffer, VkImage swapchainImage, VkRenderingFlags renderingFlags,
        VkRect2D rectRenderSize, uint32_t layerCount, uint32_t viewMask,
        std::vector<AttachmentDescription> colorAttachmentDescList,
        const AttachmentDescription* depthAttachmentDescList,
        const AttachmentDescription* stencilAttachmentDescList,
        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    static void endRenderCmd(
        VkCommandBuffer commandBuffer, VkImage swapchainImage,
        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

};

}
