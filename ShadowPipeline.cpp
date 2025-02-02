//
// Created by darby on 1/30/2025.
//

#include "ShadowPipeline.hpp"
#include "PipelineBuilder.hpp"
#include "VulkanInitUtility.hpp"
#include "VulkanFileLoaderUtility.hpp"

void ShadowPipeline::init(VkDevice device, AllocatedImage& shadow_map_image, VkDescriptorSetLayout light_source_descriptor_set_layout_) {

    this->light_source_descriptor_set_layout = light_source_descriptor_set_layout_;

    VkShaderModule shadow_vert_shader;
    if(!vk_file::load_shader_module("../shaders/shadow_map.vert.spv", device, &shadow_vert_shader)) {
        fmt::print("Error loading shadow vert shader\n");
    }

    VkShaderModule shadow_frag_shader;
    if(!vk_file::load_shader_module("../shaders/shadow_map.frag.spv", device, &shadow_frag_shader)) {
        fmt::print("Error loading shadow frag shader\n");
    }

    VkPushConstantRange buffer_range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(GPUDrawPushConstants),
    };

    std::vector<VkPushConstantRange> shadow_push_constant_ranges {
            buffer_range
    };

    std::vector<VkDescriptorSetLayout> pipeline_descriptor_set_layouts = {
            light_source_descriptor_set_layout
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = vk_init::get_pipeline_layout_create_info(pipeline_descriptor_set_layouts, shadow_push_constant_ranges);
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));

    PipelineBuilder builder;
    builder.layout = pipeline_layout;
    builder.set_shaders(shadow_vert_shader, shadow_frag_shader);
    builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder.set_rasterizer_polygon_mode(VK_POLYGON_MODE_FILL);
    // builder.set_rasterizer_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.set_rasterizer_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);
    builder.set_multisampling_none();
    builder.disable_blending();
    builder.enable_depth_test(true, VK_COMPARE_OP_LESS);
    builder.disable_color_attachment();
    builder.set_depth_format(shadow_map_image.format);

    pipeline = builder.build_pipeline(device, "Shadow Pipeline");

    vkDestroyShaderModule(device, shadow_vert_shader, nullptr);
    vkDestroyShaderModule(device, shadow_frag_shader, nullptr);

}

VkDescriptorSet ShadowPipeline::create_frame_light_data_descriptor_set(VkDevice device, LightSourceData& light_data,
                                                                       VmaAllocator& allocator,
                                                                       DescriptorAllocatorGrowable& frame_descriptor_allocator,
                                                                       DeletionQueue& frame_deletion_queue) {

    Buffer light_data_buffer;
    light_data_buffer.init(allocator,  sizeof(LightSourceData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    frame_deletion_queue.push_function([=](){
       light_data_buffer.destroy_buffer();
    });

    LightSourceData* lsd = (LightSourceData*)light_data_buffer.info.pMappedData;
    *lsd = light_data;

    VkDescriptorSet shadow_descriptor_set = frame_descriptor_allocator.allocate(device, light_source_descriptor_set_layout, nullptr);

    writer.clear();
    writer.write_buffer(0, light_data_buffer.buffer, sizeof(LightSourceData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(device, shadow_descriptor_set);

    return shadow_descriptor_set;

}


VkDescriptorSet ShadowPipeline::create_frame_shadow_map_descriptor_set(VkDevice device, AllocatedImage& shadow_map,
                                                                       VkSampler shadow_map_sampler,
                                                                       DescriptorAllocatorGrowable& frame_descriptor_allocator,
                                                                       VkDescriptorSetLayout shadow_map_descriptor_set_layout) {
    VkDescriptorSet shadow_descriptor_set = frame_descriptor_allocator.allocate(device, shadow_map_descriptor_set_layout, nullptr);

    writer.clear();
    writer.write_image(0, shadow_map.view, shadow_map_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update_set(device, shadow_descriptor_set);

    return shadow_descriptor_set;
}

void ShadowPipeline::destroy_resources(VkDevice device) {
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
}
