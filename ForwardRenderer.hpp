//
// Created by darby on 2/9/2025.
//

#pragma once

#include "Common.hpp"
#include "AllocatedImage.hpp"
#include "ShadowPipeline.hpp"
#include "ComputeEffect.hpp"
#include "EngineStats.hpp"
#include "GLTFHDRMaterial.hpp"

class ForwardRenderer {

public:
    void init(VkDevice device,
              VmaAllocator allocator,
              AllocatedImage& draw_image,
              std::shared_ptr<ShadowPipeline>& shadow_pipeline,
              VkDescriptorSetLayout scene_descriptor_set_layout,
              VkDescriptorSetLayout shadow_map_descriptor_set_layout,
              VkDescriptorSetLayout light_source_descriptor_set_layout,
              GLTFHDRMaterial& pipeline_builder);

    void destroy();

    void draw(VkCommandBuffer cmd,
              DescriptorAllocatorGrowable& frame_descriptor_allocator,
              DeletionQueue& frame_deletion_queue,
              AllocatedImage& shadow_map,
              VkSampler shadow_map_sampler,
              GPUSceneData& current_scene_data,
              EngineStats& engine_stats,
              DrawContext& draw_context,
              VkDescriptorSet* light_data_descriptor_set);

private:

    void clear_draw_image(VkCommandBuffer cmd);
    void draw_geometry_into_draw_image(VkCommandBuffer cmd,
                                       DescriptorAllocatorGrowable& frame_descriptor_allocator,
                                       DeletionQueue& frame_deletion_queue,
                                       AllocatedImage& shadow_map,
                                       VkSampler shadow_map_sampler,
                                       GPUSceneData& current_scene_data,
                                       EngineStats& engine_stats,
                                       DrawContext& draw_context,
                                       VkDescriptorSet* light_data_descriptor_set);
    void tone_map_draw_image(VkCommandBuffer cmd);

    // Pipelines


    // Shadows
    // Engine-Owned
    AllocatedImage draw_image;
    VkDevice device;
    VmaAllocator allocator;
    std::shared_ptr<ShadowPipeline> shadow_pipeline;
    VkDescriptorSetLayout scene_descriptor_set_layout;
    VkDescriptorSetLayout shadow_map_descriptor_set_layout;

    // Renderer-Owned
    VkDescriptorSetLayout draw_image_descriptor_layout;
    VkDescriptorSet draw_image_descriptor_set;

    AllocatedImage depth_image;

    DescriptorAllocatorGrowable renderer_descriptor_allocator;

    ComputePipeline tone_mapping_pipeline;
    ToneMappingComputePushConstants tone_mapping_data {
            .exposure = 1.0f,
            .tone_mapping_strategy = 0
    };

    DeletionQueue renderer_deletion_queue;

//    // Interactive state
//    std::vector<ComputeEffect> compute_effects;
//    int current_compute_effect{1};
//    float render_scale = 1.0f;
//    float ambient_occlusion_strength = 1.0f;
//    float sunlight_angle = 180.0f;
//    float sunlight_light_distance = 10.0f;
//    float shadow_bias_scalar = 0.0001f;
//    bool use_perspective_light_projection = false;
//    int shadow_softening_kernel_size = 3;
//
//    float hdr_exposure = 1.0f;
//    int tone_mapping_strategy_index = 0;
//    const char* tone_mapping_strategies[3] = {
//            "None",
//            "Simple RGB Reinhard",
//            "Hable-Filmic/Uncharted 2"
//    };

};

