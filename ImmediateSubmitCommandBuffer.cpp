//
// Created by darby on 1/6/2025.
//

#include "ImmediateSubmitCommandBuffer.hpp"
#include "VulkanInitUtility.hpp"


void ImmediateSubmitCommandBuffer::init(VkDevice _device, VkQueue _submit_queue, uint32_t queue_family_index) {
    this->device = _device;
    this->submit_queue = _submit_queue;

    // CREATE COMMAND POOL
    VkCommandPoolCreateInfo command_pool_create_info = vk_init::get_command_pool_create_info(queue_family_index);
    VK_CHECK(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool));

    // CREATE COMMAND BUFFER
    VkCommandBufferAllocateInfo command_buffer_allocate_info = vk_init::get_command_buffer_allocate_info(command_pool, 1);
    VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &command_buffer));

    // CREATE FENCE
    VkFenceCreateInfo fence_create_info = vk_init::get_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &fence));
}

void ImmediateSubmitCommandBuffer::submit(std::function<void(VkCommandBuffer)>&& function) {
    VK_CHECK(vkResetFences(device, 1, &fence));
    VK_CHECK(vkResetCommandBuffer(command_buffer, 0));

    VkCommandBuffer cmd = command_buffer;
    VkCommandBufferBeginInfo cmd_begin_info = vk_init::get_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmd_info = vk_init::get_command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit_info_2 = vk_init::get_submit_info(&cmd_info, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(submit_queue, 1, &submit_info_2, fence));

    VK_CHECK(vkWaitForFences(device, 1, &fence, true, 10000000000));
}

void ImmediateSubmitCommandBuffer::destroy() {
    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, command_pool, nullptr);
}