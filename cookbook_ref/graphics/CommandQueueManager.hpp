//
// Created by darby on 6/2/2024.
//

#pragma once

#include "volk.h"
#include "Context.hpp"

namespace Fairy::Graphics {

class CommandQueueManager {

public:
    CommandQueueManager(const Context& context, VkDevice device, VkQueue queue, uint32_t queueFamilyIndex,
                        uint32_t commandBufferCount, uint32_t commandsInFlight);

    ~CommandQueueManager();

    // VkCommandBuffer getSecondaryCmdBuffer();

    VkCommandBuffer beginCmdBuffer();

    void endCmdBuffer(VkCommandBuffer cmdBuffer);

    void submitCmdBuffer(const VkSubmitInfo* submitInfo);

    VkCommandBuffer getCmdBuffer();

    void moveToNextCmdBuffer();

    void waitUntilSubmitIsComplete();

    void waitUntilAllSubmitsAreComplete();

    void disposeWhenSubmitCompletes(std::shared_ptr<Buffer> buffer);

private:

    VkDevice device_;
    VkQueue queue_;
    uint32_t queueFamilyIndex_;

    VkCommandPool commandPool_;

    std::vector<VkCommandBuffer> commandBuffers_;
    uint32_t commandBufferCount_;
    uint32_t currentCommandBufferIndex_ = 0;

    std::vector<bool> isSubmitted_;
    std::vector<VkFence> fences_;
    uint32_t fenceCount_;
    uint32_t currentFenceIndex_ = 0;

    std::vector<std::vector<std::shared_ptr<Buffer>>> buffersToDispose_; // fenceIndex > list of buffers associated with fence to be disposed.

};

}