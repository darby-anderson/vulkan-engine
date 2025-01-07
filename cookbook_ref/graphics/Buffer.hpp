//
// Created by darby on 6/21/2024.
//
#pragma once

#include "vk_mem_alloc.h"

namespace Fairy::Graphics {

class Context;

class Texture;

class Buffer final {

public:
    // Creates staging buffer automatically (this obj is the staging buffer)
    explicit Buffer(const Context* context, VmaAllocator vmaAllocator, VkDeviceSize size, VkBufferUsageFlags usage,
                    Buffer* actualBuffer, const std::string& name = "");

    // Does not create staging buffer automatically
    explicit Buffer(const Context* context, VmaAllocator vmaAllocator, const VkBufferCreateInfo& createInfo,
                    const VmaAllocationCreateInfo& allocInfo, const std::string& name = "");

    ~Buffer();

    VkDeviceSize size() const;

    void upload(VkDeviceSize offset = 0) const;

    void upload(VkDeviceSize offset, VkDeviceSize size) const;

    void uploadStagingBufferToGPU(const VkCommandBuffer& commandBuffer, uint64_t srcOffset = 0,
                                  uint64_t dstOffset = 0) const;

    void copyDataToBuffer(const void* data, size_t size) const;

    [[nodiscard]] VkBuffer vkBuffer() const { return buffer_; }

    VkDeviceAddress vkDeviceAddress() const;

    VkBufferView requestBufferView(VkFormat viewFormat);

private:
    const Context* context_ = nullptr;
    VmaAllocator allocator_;

    VkDeviceSize size_;
    VkBufferUsageFlags usage_;
    VmaAllocationCreateInfo allocationCreateInfo_;
    VkBuffer buffer_ = VK_NULL_HANDLE;

    Buffer* targetBufferIfStaging_ = nullptr;
    VmaAllocation allocation_ = nullptr;
    VmaAllocationInfo allocationInfo_ = {};

    mutable VkDeviceAddress bufferDeviceAddress_ = 0;
    mutable void* mappedMemory_ = nullptr;
    std::unordered_map<VkFormat, VkBufferView> bufferViews_;

    const std::string name_;

};

}