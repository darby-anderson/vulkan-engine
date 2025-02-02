//
// Created by darby on 12/4/2024.
//

#pragma once

#include "Common.hpp"

namespace vk_init {

    VkCommandPoolCreateInfo get_command_pool_create_info(uint32_t family_queue_index);

    VkCommandBufferAllocateInfo get_command_buffer_allocate_info(VkCommandPool command_pool, uint32_t count);

    VkFenceCreateInfo get_fence_create_info(VkFenceCreateFlags flags);

    VkSemaphoreCreateInfo get_semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

    VkCommandBufferBeginInfo get_command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);

    VkImageSubresourceRange get_image_subresource_range(VkImageAspectFlags flags);

    VkSemaphoreSubmitInfo get_semaphore_submit_info(VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore);

    VkCommandBufferSubmitInfo get_command_buffer_submit_info(VkCommandBuffer cmd);

    VkSubmitInfo2 get_submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* waitSemaphoreInfo, VkSemaphoreSubmitInfo* signalSemaphoreInfo);

    VkImageCreateInfo get_image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent);

    VkImageViewCreateInfo get_image_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags);

    VkRenderingAttachmentInfo get_color_attachment_info(VkImageView view, VkClearValue* clear_value, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo get_depth_attachment_info(VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo get_rendering_info(VkExtent2D render_extent, const std::vector<VkRenderingAttachmentInfo>& color_attachments, VkRenderingAttachmentInfo* depth_attachment);

    VkPipelineShaderStageCreateInfo  get_pipeline_shader_stage_info(VkShaderStageFlagBits stage, VkShaderModule shader_module);

    VkPipelineLayoutCreateInfo get_pipeline_layout_create_info(const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts, const std::vector<VkPushConstantRange>& push_constant_ranges);

}

