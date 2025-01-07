//
// Created by darby on 1/4/2025.
//

#pragma once

#include "Common.hpp"

class DescriptorWriter {

public:
    std::deque<VkDescriptorImageInfo> image_infos;
    std::deque<VkDescriptorBufferInfo> buffer_infos;
    std::vector<VkWriteDescriptorSet> writes;

    void write_image(uint32_t binding, VkImageView view, VkSampler sampler, VkImageLayout layout, VkDescriptorType descriptor_type);
    void write_buffer(uint32_t binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType descriptor_type);

    void clear();
    void update_set(VkDevice device, VkDescriptorSet set);

};
