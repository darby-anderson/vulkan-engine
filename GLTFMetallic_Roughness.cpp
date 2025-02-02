//
// Created by darby on 1/9/2025.
//

#include "GLTFMetallic_Roughness.hpp"
#include "VulkanFileLoaderUtility.hpp"
#include "PipelineBuilder.hpp"
#include "DescriptorLayoutBuilder.hpp"
#include "VulkanInitUtility.hpp"

void GLTFMetallic_Roughness::build_pipelines(VkDevice device,
                                             VkDescriptorSetLayout light_data_descriptor_layout,
                                             VkDescriptorSetLayout scene_data_descriptor_layout,
                                             VkDescriptorSetLayout shadow_map_descriptor_layout,
                                             AllocatedImage& draw_image, AllocatedImage& depth_image) {
    VkShaderModule mesh_vert_shader;
    if(!vk_file::load_shader_module("../shaders/brdf_mesh.vert.spv", device, &mesh_vert_shader)) {
        fmt::print("Error loading mesh vert shader\n");
    }

    VkShaderModule mesh_frag_shader;
    if(!vk_file::load_shader_module("../shaders/brdf_mesh.frag.spv", device, &mesh_frag_shader)) {
        fmt::print("Error loading mesh frag shader\n");
    }

    VkPushConstantRange buffer_range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(GPUDrawPushConstants),
    };

    std::vector<VkPushConstantRange> mesh_push_constant_ranges {
            buffer_range
    };

    DescriptorLayoutBuilder material_layout_builder;
    material_layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    material_layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    material_layout_builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    material_layout_builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    material_layout_builder.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    material_layout = material_layout_builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    std::vector<VkDescriptorSetLayout> mesh_descriptor_set_layouts {
            scene_data_descriptor_layout,
            light_data_descriptor_layout,
            shadow_map_descriptor_layout,
            material_layout
    };

    VkPipelineLayout pipeline_layout;
    VkPipelineLayoutCreateInfo pipeline_layout_info = vk_init::get_pipeline_layout_create_info(mesh_descriptor_set_layouts, mesh_push_constant_ranges);
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));

    opaque_pipeline.layout = pipeline_layout;
    transparent_pipeline.layout = pipeline_layout;

    PipelineBuilder builder;
    builder.layout = pipeline_layout;
    builder.set_shaders(mesh_vert_shader, mesh_frag_shader);
    builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder.set_rasterizer_polygon_mode(VK_POLYGON_MODE_FILL);
    // builder.set_rasterizer_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    builder.set_rasterizer_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.set_multisampling_none();
    // builder.enable_blending_additive();
    builder.disable_blending();
    // builder.enable_depth_test(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    builder.enable_depth_test(true, VK_COMPARE_OP_LESS);
    // builder.disable_depth_test();

    builder.set_color_attachment_format(draw_image.format);
    builder.set_depth_format(depth_image.format); // ORIGINAL
    // builder.set_depth_format(VK_FORMAT_UNDEFINED);

    opaque_pipeline.pipeline = builder.build_pipeline(device, "Opaque Pipeline");

    builder.enable_blending_additive();

    transparent_pipeline.pipeline = builder.build_pipeline(device, "Transparent Pipeline");

    vkDestroyShaderModule(device, mesh_vert_shader, nullptr);
    vkDestroyShaderModule(device, mesh_frag_shader, nullptr);

}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device, MaterialPassType pass_type,
                                                        const GLTFMetallic_Roughness::MaterialResources& resources,
                                                        DescriptorAllocatorGrowable& descriptor_allocator) {

    MaterialInstance mat_data;
    mat_data.pass_type = pass_type;
    if(pass_type == MaterialPassType::Transparent) {
        mat_data.pipeline = &transparent_pipeline;
    } else {
        mat_data.pipeline = &opaque_pipeline;
    }

    mat_data.material_set = descriptor_allocator.allocate(device, material_layout, nullptr);

    writer.clear();
    writer.write_buffer(0, resources.data_buffer, sizeof(MaterialConstants), resources.data_buffer_offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(1, resources.color_image.view, resources.color_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2, resources.metal_rough_image.view, resources.metal_rough_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(3, resources.normal_image.view, resources.normal_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(4, resources.ambient_occlusion_image.view, resources.ambient_occlusion_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update_set(device, mat_data.material_set);

    return mat_data;
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device) {
    vkDestroyPipelineLayout(device, opaque_pipeline.layout, nullptr);
    vkDestroyPipeline(device, opaque_pipeline.pipeline, nullptr);

    vkDestroyPipelineLayout(device, transparent_pipeline.layout, nullptr);
    vkDestroyPipeline(device, transparent_pipeline.pipeline, nullptr);

    vkDestroyDescriptorSetLayout(device, material_layout, nullptr);
}
