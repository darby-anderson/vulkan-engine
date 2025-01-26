//
// Created by darby on 1/9/2025.
//

#pragma once

#include "Common.hpp"
#include "GraphicsTypes.hpp"
#include "AllocatedImage.hpp"
#include "DescriptorWriter.hpp"
#include "DescriptorAllocatorGrowable.hpp"

class GLTFMetallic_Roughness {

    DescriptorWriter writer;

public:

    MaterialPipeline opaque_pipeline;
    MaterialPipeline transparent_pipeline;

    VkDescriptorSetLayout material_layout; // created when building the pipelines


    struct MaterialConstants {
        glm::vec4 color_factors;
        glm::vec4 metal_rough_factors;

        glm::vec4 padding[14]; // padding for uniform buffers
    };

    struct MaterialResources {
        AllocatedImage color_image;
        VkSampler color_sampler;

        AllocatedImage metal_rough_image;
        VkSampler metal_rough_sampler;

        VkBuffer data_buffer;
        uint32_t data_buffer_offset;
    };


    void build_pipelines(VkDevice device, VkDescriptorSetLayout scene_data_descriptor_layout, AllocatedImage& draw_image, AllocatedImage& depth_image);
    void clear_resources(VkDevice device);

    MaterialInstance write_material(VkDevice device, MaterialPassType pass_type, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptor_allocator);

};

