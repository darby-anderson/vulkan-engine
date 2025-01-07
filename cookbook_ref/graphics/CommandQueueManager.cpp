//
// Created by darby on 6/2/2024.
//

#include <tracy/Tracy.hpp>

#include "CommandQueueManager.hpp"
#include "VulkanUtils.hpp"
#include "Context.hpp"

namespace Fairy::Graphics {

CommandQueueManager::CommandQueueManager(const Context& context, VkDevice device, VkQueue queue, uint32_t queueFamilyIndex, uint32_t commandBufferCount, uint32_t commandsInFlight)
    : device_(device), queue_(queue), queueFamilyIndex_(queueFamilyIndex), commandBufferCount_(commandBufferCount), fenceCount_(commandsInFlight) {


    isSubmitted_.reserve(fenceCount_);
    fences_.reserve(fenceCount_);
    buffersToDispose_.resize(fenceCount_);

    const VkCommandPoolCreateInfo vpci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIndex,
    };
    VK_CHECK(vkCreateCommandPool(device, &vpci, nullptr, &commandPool_));
    context.setVkObjectName(commandPool_, VK_OBJECT_TYPE_COMMAND_POOL, "Command Pool");

    // Create the command buffers
    const VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    commandBuffers_.reserve(commandBufferCount_);
    for(uint32_t i = 0; i < commandBufferCount_; i++) {
        VkCommandBuffer cmdBuf;
        VK_CHECK(vkAllocateCommandBuffers(device, &cbai, &cmdBuf));
        context.setVkObjectName(cmdBuf, VK_OBJECT_TYPE_COMMAND_BUFFER, "Command Buffer " + std::to_string(i));

        commandBuffers_.push_back(cmdBuf);
    }

    for(uint32_t i = 0; i < fenceCount_; i++) {
        const VkFenceCreateInfo fci =
        {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        VkFence fence;
        VK_CHECK(vkCreateFence(device_, &fci, nullptr, &fence));

        context.setVkObjectName(fence, VK_OBJECT_TYPE_FENCE, "command queue fence " + std::to_string(i));

        fences_.push_back(fence);
        isSubmitted_.push_back(false);
    }
}


CommandQueueManager::~CommandQueueManager() {
    for(uint32_t i = 0; i < fenceCount_; i++) {
        VK_CHECK(vkWaitForFences(device_, 1, &fences_[i], VK_TRUE, UINT64_MAX));
        vkDestroyFence(device_, fences_[i], nullptr);
    }

    for(uint32_t i = 0; i < commandBufferCount_; i++) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffers_[i]);
    }

    vkDestroyCommandPool(device_, commandPool_, nullptr);
}


VkCommandBuffer CommandQueueManager::beginCmdBuffer() {
    ZoneScopedN("CmdMgr: beginCmdBuffer");
    VK_CHECK(vkWaitForFences(device_, 1, &fences_[currentFenceIndex_], true, UINT32_MAX));
    VK_CHECK(vkResetCommandBuffer(commandBuffers_[currentCommandBufferIndex_], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));

    const VkCommandBufferBeginInfo bi = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            //.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(commandBuffers_[currentCommandBufferIndex_], &bi));

    return commandBuffers_[currentCommandBufferIndex_];
}

void CommandQueueManager::endCmdBuffer(VkCommandBuffer cmdBuffer) {
    ZoneScopedN("CmdMgr: endCmdBuffer");
    VK_CHECK(vkEndCommandBuffer(cmdBuffer));
}

void CommandQueueManager::submitCmdBuffer(const VkSubmitInfo* submitInfo) {
    ZoneScopedN("CmdMgr: submitCmdBuffer");
    VK_CHECK(vkResetFences(device_, 1, &fences_[currentFenceIndex_]));
    VK_CHECK(vkQueueSubmit(queue_, 1, submitInfo, fences_[currentFenceIndex_]));
    isSubmitted_[currentFenceIndex_] = true;
}

/*
 * Circularly moves along a ring of command buffers and fences
 */
void CommandQueueManager::moveToNextCmdBuffer() {
    currentFenceIndex_ = (currentFenceIndex_ + 1) % fenceCount_;
    currentCommandBufferIndex_ = (currentCommandBufferIndex_ + 1) % commandBufferCount_;
}

VkCommandBuffer CommandQueueManager::getCmdBuffer() {
    ZoneScopedN("CmdMgr: getCmdBuffer");

    const VkCommandBufferAllocateInfo commandBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmdBuffer {VK_NULL_HANDLE};
    VK_CHECK(vkAllocateCommandBuffers(device_, &commandBufferInfo, &cmdBuffer));

    return cmdBuffer;
}

void CommandQueueManager::disposeWhenSubmitCompletes(std::shared_ptr<Buffer> buffer) {
    ZoneScopedN("CmdMgr: disposeWhenSubmitCompletes");
    buffersToDispose_[currentFenceIndex_].push_back(std::move(buffer));
}

void CommandQueueManager::waitUntilSubmitIsComplete() {
    ZoneScopedN("CmdMgr: waitUntilSubmitIsComplete");

    if(!isSubmitted_[currentFenceIndex_]) {
        return;
    }

    const auto result = vkWaitForFences(device_, 1, &fences_[currentFenceIndex_], true, UINT32_MAX);

    if(result == VK_TIMEOUT) {
        std::cerr << "Timeout waiting for submit to complete" << std::endl;
        vkDeviceWaitIdle(device_);
    }

    isSubmitted_[currentFenceIndex_] = false;
    buffersToDispose_[currentFenceIndex_].clear();
}

void CommandQueueManager::waitUntilAllSubmitsAreComplete() {
    ZoneScopedN("CmdMgr: waitUntilAllSubmitsAreComplete");
    for(size_t index = 0; auto& fence : fences_) {
        VK_CHECK(vkWaitForFences(device_, 1, &fence, true, UINT32_MAX));
        VK_CHECK(vkResetFences(device_, 1, &fence));
        isSubmitted_[index++] = false;
    }
    buffersToDispose_.clear();
}

}