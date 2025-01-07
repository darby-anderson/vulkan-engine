//
// Created by darby on 12/11/2024.
//

#pragma once

#include "Common.hpp"
#include "DeletionQueue.hpp"

class ComputePipeline {

public:
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    void init(VkDevice device,
              const char* spv_path,
              const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts,
              const std::vector<VkPushConstantRange>& push_constant_ranges,
              DeletionQueue& main_deletion_queue);

};

