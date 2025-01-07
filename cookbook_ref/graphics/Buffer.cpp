//
// Created by darby on 6/21/2024.
//
#include "Context.hpp"

#include "Buffer.hpp"

namespace Fairy::Graphics {

// Creates
Buffer::Buffer(const Context* context, VmaAllocator vmaAllocator, VkDeviceSize size, VkBufferUsageFlags usage,
               Buffer* actualBuffer, const std::string& name)
    : context_(context),
      allocator_(vmaAllocator),
      size_(size),
      targetBufferIfStaging_(actualBuffer),
      name_(name) {

    ASSERT(targetBufferIfStaging_, "We're using staging buffer, so actualBufferIfStaging_ can't be null");
    ASSERT(targetBufferIfStaging_->usage_ & VK_BUFFER_USAGE_TRANSFER_DST_BIT, "Actual buffer must be transfer dst");
    ASSERT(targetBufferIfStaging_->allocationCreateInfo_.usage == VMA_MEMORY_USAGE_GPU_ONLY,
           "Actual buffer must be device-only ");

    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .size = size_,
        .usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = {},
        .pQueueFamilyIndices = {}
    };

    allocationCreateInfo_ = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_ONLY
    };

    VK_CHECK(vmaCreateBuffer(allocator_, &createInfo, &allocationCreateInfo_, &buffer_, &allocation_, &allocationInfo_));
    context_->setVkObjectName(buffer_, VK_OBJECT_TYPE_BUFFER, "Buffer: " + name);
}


Buffer::Buffer(const Context* context, VmaAllocator vmaAllocator, const VkBufferCreateInfo& createInfo,
               const VmaAllocationCreateInfo& allocInfo, const std::string& name)
    : context_(context),
      allocator_(vmaAllocator),
      size_(createInfo.size),
      usage_(createInfo.usage),
      allocationCreateInfo_(allocInfo),
      name_(name) {

    VK_CHECK(vmaCreateBuffer(allocator_, &createInfo, &allocInfo, &buffer_, &allocation_, &allocationInfo_));
    context_->setVkObjectName(buffer_, VK_OBJECT_TYPE_BUFFER, "Buffer: " + name);
}

Buffer::~Buffer() {
    if (mappedMemory_) {
        vmaUnmapMemory(allocator_, allocation_);
    }

    for (auto& [bufferViewFormat, bufferView]: bufferViews_) {
        vkDestroyBufferView(context_->device(), bufferView, nullptr);
    }

    vmaDestroyBuffer(allocator_, buffer_, allocation_);
}

VkDeviceSize Buffer::size() const {
    return size_;
}

void Buffer::upload(VkDeviceSize offset) const {
    upload(offset, size_);
}

void Buffer::upload(VkDeviceSize offset, VkDeviceSize size) const {
    VK_CHECK(vmaFlushAllocation(allocator_, allocation_, offset, size));
}

void
Buffer::uploadStagingBufferToGPU(VkCommandBuffer const& commandBuffer, uint64_t srcOffset, uint64_t dstOffset) const {
    ASSERT(targetBufferIfStaging_ != nullptr, "target buffer cannot be null when uploading to GPU");

    VkBufferCopy region{
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = size_
    };

    vkCmdCopyBuffer(commandBuffer, vkBuffer(), targetBufferIfStaging_->vkBuffer(), 1, &region);
}

void Buffer::copyDataToBuffer(const void* data, size_t size) const {

    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(allocator_, allocation_, &allocInfo);
    if(allocInfo.pMappedData != mappedMemory_) {
        std::cout << "Issue with mapped data" << std::endl;
    }

    VkMemoryPropertyFlags flags;
    vmaGetAllocationMemoryProperties(allocator_, allocation_, &flags);
//    if(flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
//        std::cout << "host visible" << std::endl;
//    }
//
//    if(flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
//        std::cout << "host coherent" << std::endl;
//    }

    if (!mappedMemory_) {
        VK_CHECK(vmaMapMemory(allocator_, allocation_, &mappedMemory_));
    }

    /*uint8_t block_of_ones[size];
    memset(block_of_ones, 0xFF, size);*/

    memcpy(mappedMemory_, data, size);

    vmaFlushAllocation(allocator_, allocation_, 0, size);
}

VkDeviceAddress Buffer::vkDeviceAddress() const {
    if (targetBufferIfStaging_) {
        return targetBufferIfStaging_->vkDeviceAddress();
    }

#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
    if (!bufferDeviceAddress_) {
        const VkBufferDeviceAddressInfo bdAddressInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer_
        };
        bufferDeviceAddress_ = vkGetBufferDeviceAddress(context_->device(), &bdAddressInfo);
    }

    return bufferDeviceAddress_;
#else
    return 0;
#endif
}

VkBufferView Buffer::requestBufferView(VkFormat viewFormat) {

    auto itr = bufferViews_.find(viewFormat);

    if (itr != bufferViews_.end()) {
        return itr->second;
    }

    VkBufferViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .flags = 0,
        .buffer = vkBuffer(),
        .format = viewFormat,
        .offset = 0,
        .range = size_
    };

    VkBufferView bufferView;
    VK_CHECK(vkCreateBufferView(context_->device(), &createInfo, nullptr, &bufferView));
    bufferViews_[viewFormat] = bufferView;
    return bufferView;
}

}