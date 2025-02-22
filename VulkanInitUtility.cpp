//
// Created by darby on 12/17/2024.
//

#include "VulkanInitUtility.hpp"

namespace vk_init {

    VkCommandPoolCreateInfo get_command_pool_create_info(uint32_t family_queue_index) {
        VkCommandPoolCreateInfo command_pool_create_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = family_queue_index,
        };

        return command_pool_create_info;
    }

    VkCommandBufferAllocateInfo get_command_buffer_allocate_info(VkCommandPool command_pool, uint32_t count) {
        VkCommandBufferAllocateInfo command_buffer_allocate_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = count,
        };

        return command_buffer_allocate_info;
    }

    VkFenceCreateInfo get_fence_create_info(VkFenceCreateFlags flags) {
        VkFenceCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = flags
        };

        return info;
    }

    VkSemaphoreCreateInfo get_semaphore_create_info(VkSemaphoreCreateFlags flags) {
        VkSemaphoreCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = flags
        };

        return info;
    }

    VkCommandBufferBeginInfo get_command_buffer_begin_info(VkCommandBufferUsageFlags flags) {
        VkCommandBufferBeginInfo info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = flags
        };

        return info;
    }

    VkImageSubresourceRange get_image_subresource_range(VkImageAspectFlags flags) {
        VkImageSubresourceRange range = {
                .aspectMask = flags,
                .baseMipLevel = 0,                      // Default will transition all array layers and mip levels VVV
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS
        };

        return range;
    }

    VkSemaphoreSubmitInfo get_semaphore_submit_info(VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore) {
        VkSemaphoreSubmitInfo info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = semaphore,
                .value = 1,
                .stageMask = stage_mask,
                .deviceIndex = 0,
        };

        return info;
    }

    VkCommandBufferSubmitInfo get_command_buffer_submit_info(VkCommandBuffer cmd) {
        VkCommandBufferSubmitInfo info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd,
                .deviceMask = 0
        };

        return info;
    }

    VkSubmitInfo2 get_submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* waitSemaphoreInfo, VkSemaphoreSubmitInfo* signalSemaphoreInfo) {
        VkSubmitInfo2 info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,

                .waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0u : 1u,
                .pWaitSemaphoreInfos = waitSemaphoreInfo,

                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = cmd,

                .signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0u : 1u,
                .pSignalSemaphoreInfos = signalSemaphoreInfo,
        };

        return info;
    }

    VkImageCreateInfo get_image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent) {
        VkImageCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = nullptr,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = format,
                .extent = extent,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = usage_flags,
        };

        return info;
    }

    VkImageViewCreateInfo get_image_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags) {
        VkImageSubresourceRange range = {
                .aspectMask = aspect_flags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
        };

        VkImageViewCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format,
                .subresourceRange = range
        };

        return info;
    }

    VkRenderingAttachmentInfo get_color_attachment_info(VkImageView view, VkClearValue* clear_value, VkImageLayout layout) {
        VkRenderingAttachmentInfo info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = view,
                .imageLayout = layout,
                .loadOp = clear_value ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        if(clear_value) {
            info.clearValue = *clear_value;
        }

        return info;
    }

    VkRenderingAttachmentInfo get_depth_attachment_info(VkImageView view, VkImageLayout layout) {
        VkRenderingAttachmentInfo info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = view,
                .imageLayout = layout,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {
                        .depthStencil = {
                                .depth = 1.f
                        }
                }
        };

        return info;
    }


    VkRenderingInfo get_rendering_info(VkExtent2D render_extent, const std::vector<VkRenderingAttachmentInfo>& color_attachments, VkRenderingAttachmentInfo* depth_attachment) {
        uint32_t color_attachment_count = static_cast<uint32_t>(color_attachments.size());

        VkRenderingInfo info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .pNext = nullptr,
                .renderArea = VkRect2D { VkOffset2D{0, 0}, render_extent },
                .layerCount = 1,
                .colorAttachmentCount = color_attachment_count,
                .pColorAttachments = color_attachment_count == 0 ? nullptr : color_attachments.data(),
                .pDepthAttachment = depth_attachment,
                .pStencilAttachment = nullptr
        };

        return info;
    }

    VkPipelineLayoutCreateInfo get_pipeline_layout_create_info(const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts, const std::vector<VkPushConstantRange>& push_constant_ranges) {

        uint32_t descriptor_set_layout_count = static_cast<uint32_t>(descriptor_set_layouts.size());
        uint32_t push_constant_range_count = static_cast<uint32_t>(push_constant_ranges.size());

        VkPipelineLayoutCreateInfo compute_layout = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .setLayoutCount = descriptor_set_layout_count,
                .pSetLayouts = descriptor_set_layout_count == 0 ? nullptr : descriptor_set_layouts.data(),
                .pushConstantRangeCount = push_constant_range_count,
                .pPushConstantRanges = push_constant_range_count == 0 ? nullptr : push_constant_ranges.data()
        };

        return compute_layout;
    }

    VkPipelineShaderStageCreateInfo get_pipeline_shader_stage_info(VkShaderStageFlagBits stage, VkShaderModule shader_module) {
        VkPipelineShaderStageCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .stage = stage,
                .module = shader_module,
                .pName = "main"
        };

        return info;
    }

}