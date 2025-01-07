//
// Created by darby on 12/19/2024.
//

#include "Buffer.hpp"

#include "VulkanDebugUtility.hpp"

/*
 * VMA USAGES
 *
 * GPU ONLY - Fastest to be read on the GPU, not accessible to read/write from CPU. VRAM, device-local
 * CPU ONLY - Memory we can write from the CPU, and GPU can read (albeit slowly). CPU RAM, host-local
 * CPU TO GPU - Maybe faster to read from GPU than CPU ONLY. Limited size.
 * GPU TO CPU - We want to be able to read this data from the CPU
 */

void Buffer::init(VmaAllocator _allocator, size_t alloc_size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage) {

    this->allocator = _allocator;

    VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = alloc_size,
            .usage = buffer_usage
    };

    VmaAllocationCreateInfo alloc_info = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = memory_usage,
    };

    VK_CHECK(vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &buffer, &allocation, &info));
}

void Buffer::set_name(VkDevice device, const char* name) {
    vk_debug::name_resource<VkBuffer>(device, VK_OBJECT_TYPE_BUFFER, buffer, name);
}

void Buffer::destroy_buffer() const {
    vmaDestroyBuffer(allocator, buffer, allocation);
}

