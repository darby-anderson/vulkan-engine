//
// Created by darby on 1/9/2025.
//

#pragma once

#include "Common.hpp"
#include "GraphicsTypes.hpp"
#include "AllocatedImage.hpp"
#include "DescriptorWriter.hpp"
#include "DescriptorAllocatorGrowable.hpp"

class GLTFHDRMaterial {

private:
    DescriptorWriter writer;

public:

    VkDescriptorSetLayout material_layout = VK_NULL_HANDLE; // created when building the pipelines

    struct ForwardRendererData {
        MaterialPipeline opaque_pipeline;
        MaterialPipeline transparent_pipeline;
    };

    GLTFHDRMaterial::ForwardRendererData forward_renderer_data;

    struct DeferredRendererData {
        MaterialPipeline geometry_pipeline;
        MaterialPipeline light_pipeline;
    };

    GLTFHDRMaterial::DeferredRendererData deferred_renderer_data;

    struct MaterialConstants {
        glm::vec4 color_factors;
        glm::vec4 metal_rough_factors;
        float normal_scale;
        float ambient_occlusion_strength;

        // [0] - includes color texture
        // [1] - includes metal_rough texture
        // [2] - includes normal texture
        // [3] - includes ambient occlusion texture
        glm::bvec4 includes_certain_textures;

        float padding1;
        glm::vec4 padding[13]; // padding for uniform buffers
    };

    struct MaterialResources {
        AllocatedImage color_image;
        VkSampler color_sampler;

        AllocatedImage metal_rough_image;
        VkSampler metal_rough_sampler;

        AllocatedImage normal_image;
        VkSampler normal_sampler;

        AllocatedImage ambient_occlusion_image;
        VkSampler ambient_occlusion_sampler;

        VkBuffer data_buffer;
        uint32_t data_buffer_offset;
    };

    void build_shared_resources(VkDevice device);

    void build_forward_renderer_pipelines(VkDevice device,
                                          VkDescriptorSetLayout light_data_descriptor_layout,
                                          VkDescriptorSetLayout scene_data_descriptor_layout,
                                          VkDescriptorSetLayout shadow_map_descriptor_layout,
                                          AllocatedImage& draw_image,
                                          AllocatedImage& depth_image);

    void build_deferred_renderer_pipelines(VkDevice device,
                                           VkDescriptorSetLayout light_data_descriptor_layout,
                                           VkDescriptorSetLayout scene_data_descriptor_layout,
                                           VkDescriptorSetLayout shadow_map_descriptor_layout,
                                           VkDescriptorSetLayout g_buffer_descriptor_layout,
                                           std::vector<AllocatedImage>& g_buffers,
                                           AllocatedImage& depth_g_buffer,
                                           AllocatedImage& draw_image);

    void clear_resources(VkDevice device);

    MaterialInstance write_material(VkDevice device, MaterialPassType pass_type, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptor_allocator);

};

