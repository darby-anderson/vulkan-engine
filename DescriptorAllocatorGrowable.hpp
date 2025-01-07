//
// Created by darby on 12/11/2024.
//

#pragma once

#include "Common.hpp"

/*
 * A wrapper around a vk descriptor pool.
 * Includes functionality to allocate a descriptor from the pool if provided a Descriptor
 * Set Layout.
 */
class DescriptorAllocatorGrowable {

public:

    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio; // percentage of pool used by this type of descriptor
    };

    void init_allocator(VkDevice device, uint32_t initial_sets, std::span<PoolSizeRatio> pool_ratios);
    void clear_descriptor_pools(VkDevice device);
    void destroy_descriptor_pools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext);

private:
    VkDescriptorPool get_pool(VkDevice device);
    VkDescriptorPool create_pool(VkDevice device, uint32_t set_count, std::span<PoolSizeRatio> pool_ratios);

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> full_pools;
    std::vector<VkDescriptorPool> ready_pools;
    uint32_t sets_per_pool; // increases as we allocate more pools. Will more gracefully handle large allocations that way.
};
