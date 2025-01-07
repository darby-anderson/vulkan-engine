//
// Created by darby on 1/4/2025.
//

#include "DescriptorWriter.hpp"

void DescriptorWriter::write_image(uint32_t binding, VkImageView view, VkSampler sampler, VkImageLayout layout,
                                   VkDescriptorType descriptor_type) {

    VkDescriptorImageInfo info = {
            .sampler = sampler,
            .imageView = view,
            .imageLayout = layout
    };
    VkDescriptorImageInfo& ref = image_infos.emplace_back(info); // need to use this ref as info goes out of scope and pointer will become invalid. the valid version will exist inside of image_infos

    VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = VK_NULL_HANDLE, // set immediately before update in update_set()
            .dstBinding = binding,
            .descriptorCount = 1,
            .descriptorType = descriptor_type,
            .pImageInfo = &ref
    };
    writes.push_back(write);
}

void DescriptorWriter::write_buffer(uint32_t binding, VkBuffer buffer, size_t size, size_t offset,
                                    VkDescriptorType descriptor_type) {

    VkDescriptorBufferInfo info = {
            .buffer = buffer,
            .offset = offset,
            .range = size,
    };
    VkDescriptorBufferInfo& ref = buffer_infos.emplace_back(info);

    VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = VK_NULL_HANDLE, // set immediately before update in update_set()
            .dstBinding = binding,
            .descriptorCount = 1,
            .descriptorType = descriptor_type,
            .pBufferInfo = &ref
    };
    writes.push_back(write);
}

void DescriptorWriter::clear() {
    image_infos.clear();
    buffer_infos.clear();
    writes.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set) {
    for(VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}


