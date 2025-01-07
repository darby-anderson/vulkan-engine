//
// Created by darby on 12/19/2024.
//

#pragma once

#include "Common.hpp"
#include "DeletionQueue.hpp"

class Buffer {

public:
    VmaAllocator allocator;

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;

    void init(VmaAllocator allocator, size_t alloc_size, VkBufferUsageFlags flags, VmaMemoryUsage memory_usage);
    void set_name(VkDevice device, const char* name);
    void destroy_buffer() const;

};

