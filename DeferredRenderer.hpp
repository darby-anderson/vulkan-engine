//
// Created by darby on 2/10/2025.
//

#pragma once

#include "Common.hpp"
#include "AllocatedImage.hpp"
#include "ShadowPipeline.hpp"
#include "ComputeEffect.hpp"
#include "EngineStats.hpp"
#include "GLTFHDRMaterial.hpp"


class DeferredRenderer {

public:
    void init(VkDevice device, VmaAllocator allocator, AllocatedImage& draw_image,
              std::shared_ptr<ShadowPipeline>& shadow_pipeline,
              VkDescriptorSetLayout scene_descriptor_set_layout,
              VkDescriptorSetLayout shadow_map_descriptor_set_layout,
              VkDescriptorSetLayout light_source_descriptor_set_layout,
              GLTFHDRMaterial& hdr_mat_pipeline_builder,
              ImmediateSubmitCommandBuffer& immediate_submit_command_buffer);


    void draw(VkCommandBuffer cmd, VkCommandBuffer cmd2,
              DescriptorAllocatorGrowable& frame_descriptor_allocator,
              DeletionQueue& frame_deletion_queue, AllocatedImage& shadow_map,
              VkSampler shadow_map_sampler, VkSampler g_buffer_sampler, GPUSceneData& current_scene_data,
              EngineStats& engine_stats, DrawContext& draw_context,
              VkDescriptorSet* p_light_data_descriptor_set);

private:

    // Engine-owned
    AllocatedImage draw_image; // will be copied to the swapchain
    VkDevice device;
    VmaAllocator allocator;
    std::shared_ptr<ShadowPipeline> shadow_pipeline;
    VkDescriptorSetLayout scene_descriptor_set_layout;
    VkDescriptorSetLayout shadow_map_descriptor_set_layout;

    // Renderer-owned

    // G-buffers
    AllocatedImage depth_g_buffer; // can be used to also determine the world position of the fragment
    AllocatedImage world_normal_g_buffer;
    AllocatedImage albedo_g_buffer;

    GPUMeshBuffers lighting_pass_triangle_buffer;

    DescriptorAllocatorGrowable renderer_descriptor_allocator;

    GLTFHDRMaterial::DeferredRendererData deferred_renderer_data;

    // Lighting descriptor sets
    VkDescriptorSetLayout lighting_pass_lighting_descriptor_set_layout;

    void create_g_buffer(AllocatedImage& g_buffer, VkExtent3D extent, const std::string& name);
    void clear_image_resources(VkCommandBuffer cmd);
    void draw_geometry_into_g_buffers(VkCommandBuffer cmd,
                                     DescriptorAllocatorGrowable& frame_descriptor_allocator,
                                     DeletionQueue& frame_deletion_queue,
                                     GPUSceneData& current_scene_data,
                                     EngineStats& engine_stats,
                                     DrawContext& draw_context);

    void draw_lighting_pass(VkCommandBuffer cmd, DescriptorAllocatorGrowable& frame_descriptor_allocator,
                            VkDescriptorSet* light_data_descriptor_set, VkSampler g_buffer_sampler,
                            DeletionQueue& frame_deletion_queue, GPUSceneData& current_scene_data,
                            EngineStats& engine_stats);


};

