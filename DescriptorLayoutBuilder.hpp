//
// Created by darby on 12/11/2024.
//

#pragma once

#include "Common.hpp"

/*
 * Helper struct that creates a descriptor set layout after adding specified bindings.
 */
struct DescriptorLayoutBuilder {

    // the bindings in shaders
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type) {
        VkDescriptorSetLayoutBinding new_binding = {
                .binding = binding,
                .descriptorType = type,
                .descriptorCount = 1,
        };

        bindings.push_back(new_binding);
    }

    void clear() {
        bindings.clear();
    }

    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0) {

        // prepare all bindings to be used in these shader stages
        for(auto& b : bindings) {
            b.stageFlags |= shaderStages;
        }

        VkDescriptorSetLayoutCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = pNext,
                .flags = flags,
                .bindingCount = (uint32_t)bindings.size(),
                .pBindings = bindings.data(),
        };

        VkDescriptorSetLayout set;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

        return set;
    }

};
