//
// Created by darby on 1/6/2025.
//

#pragma once

#include "Common.hpp"

class ImmediateSubmitCommandBuffer {

    VkDevice device;
    VkQueue submit_queue;

    VkFence fence;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;

public:
    void init(VkDevice device, VkQueue submit_queue, uint32_t queue_family_index);
    void submit(std::function<void(VkCommandBuffer)>&& function);
    void destroy();

};

