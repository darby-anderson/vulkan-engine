//
// Created by darby on 12/11/2024.
//

#include "DescriptorAllocatorGrowable.hpp"


void DescriptorAllocatorGrowable::init_allocator(VkDevice device, uint32_t initial_sets, std::span<PoolSizeRatio> pool_ratios) {

    // copy over ratios
    ratios.clear();
    for(auto r : pool_ratios) {
        ratios.push_back(r);
    }

    VkDescriptorPool new_pool = create_pool(device, initial_sets, ratios);
    sets_per_pool = initial_sets * 1.5f; // grow sets_per_pool in preparation for next allocation, in case it will become larger
    ready_pools.push_back(new_pool);
}

void DescriptorAllocatorGrowable::clear_descriptor_pools(VkDevice device) {
    for(auto p : ready_pools) {
        vkResetDescriptorPool(device, p, 0);
    }

    for(auto p : full_pools) {
        vkResetDescriptorPool(device, p, 0);
        ready_pools.push_back(p);
    }
    full_pools.clear();
}

void DescriptorAllocatorGrowable::destroy_descriptor_pools(VkDevice device) {
    for(auto p : ready_pools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    ready_pools.clear();

    for(auto p : full_pools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    full_pools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext) {

    VkDescriptorPool pool_to_use = get_pool(device);

    VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = pNext,
            .descriptorPool = pool_to_use,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout
    };

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &alloc_info, &ds);

    // Case when pool runs out of space for our allocation
    if(result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        full_pools.push_back(pool_to_use);

        VkDescriptorPool next_pool_to_use = get_pool(device);
        alloc_info.descriptorPool = next_pool_to_use;
        VkResult result2 = vkAllocateDescriptorSets(device, &alloc_info, &ds);
        fmt::print(stderr, "allocating descriptor sets. result: {}\n", static_cast<long long>(result2));
        VK_CHECK(result2);
    }

    ready_pools.push_back(pool_to_use); // need to push our latest pool back since get_pool pops it out of the vector

    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device) {
    VkDescriptorPool new_pool;
    if(ready_pools.size() != 0) {
        new_pool = ready_pools.back();
        ready_pools.pop_back();
    } else {
        new_pool = create_pool(device, sets_per_pool, ratios);

        sets_per_pool *= 1.5f;

        if(sets_per_pool > 4092) {
            sets_per_pool = 4092;
        }
    }

    return new_pool;
}

VkDescriptorPool
DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t set_count, std::span<PoolSizeRatio> pool_ratios) {

    std::vector<VkDescriptorPoolSize> pool_sizes;
    for(PoolSizeRatio ratio : pool_ratios) {
        pool_sizes.push_back(VkDescriptorPoolSize{
                .type = ratio.type,
                .descriptorCount = uint32_t(ratio.ratio * set_count)
        });
    }

    VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = 0,
            .maxSets = set_count,
            .poolSizeCount = (uint32_t)pool_sizes.size(),
            .pPoolSizes = pool_sizes.data()
    };

    VkDescriptorPool new_pool;
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &new_pool));
    return new_pool;
}


