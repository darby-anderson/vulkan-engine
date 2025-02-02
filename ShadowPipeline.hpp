//
// Created by darby on 1/30/2025.
//

#pragma once

#include "Common.hpp"
#include "DeletionQueue.hpp"
#include "AllocatedImage.hpp"
#include "DescriptorWriter.hpp"
#include "Buffer.hpp"
#include "DescriptorAllocatorGrowable.hpp"
#include "GraphicsTypes.hpp"

class ShadowPipeline {

public:


    void init(VkDevice device,
              AllocatedImage& shadow_map_image,
              VkDescriptorSetLayout light_source_descriptor_set_layout);

    VkDescriptorSet create_frame_light_data_descriptor_set(VkDevice device, LightSourceData& light_data,
                                                           VmaAllocator& allocator,
                                                           DescriptorAllocatorGrowable& frame_descriptor_allocator,
                                                           DeletionQueue& frame_deletion_queue);

    VkDescriptorSet create_frame_shadow_map_descriptor_set(VkDevice device, AllocatedImage& shadow_map,
                                           VkSampler shadow_map_sampler,
                                           DescriptorAllocatorGrowable& frame_descriptor_allocator,
                                           VkDescriptorSetLayout shadow_map_descriptor_set_layout);

    void destroy_resources(VkDevice device);

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

private:

    DescriptorWriter writer;
    VkDescriptorSetLayout light_source_descriptor_set_layout;

};
