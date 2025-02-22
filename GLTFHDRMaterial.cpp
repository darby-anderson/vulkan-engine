//
// Created by darby on 1/9/2025.
//

#include "GLTFHDRMaterial.hpp"
#include "VulkanFileLoaderUtility.hpp"
#include "PipelineBuilder.hpp"
#include "DescriptorLayoutBuilder.hpp"
#include "VulkanInitUtility.hpp"

void GLTFHDRMaterial::build_shared_resources(VkDevice device) {
    DescriptorLayoutBuilder material_layout_builder;
    material_layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    material_layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    material_layout_builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    material_layout_builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    material_layout_builder.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    material_layout = material_layout_builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}


void GLTFHDRMaterial::build_forward_renderer_pipelines(VkDevice device,
                                                       VkDescriptorSetLayout light_data_descriptor_layout,
                                                       VkDescriptorSetLayout scene_data_descriptor_layout,
                                                       VkDescriptorSetLayout shadow_map_descriptor_layout,
                                                       AllocatedImage& draw_image, AllocatedImage& depth_image) {

    if(material_layout == VK_NULL_HANDLE) {
        fmt::print("Shared resources not built before attempting to build forward renderer pipelines");
        return;
    }

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

    std::vector<VkDescriptorSetLayout> mesh_descriptor_set_layouts {
            scene_data_descriptor_layout,
            light_data_descriptor_layout,
            shadow_map_descriptor_layout,
            material_layout
    };

    VkPipelineLayout pipeline_layout;
    VkPipelineLayoutCreateInfo pipeline_layout_info = vk_init::get_pipeline_layout_create_info(mesh_descriptor_set_layouts, mesh_push_constant_ranges);
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));

    forward_renderer_data.opaque_pipeline.layout = pipeline_layout;
    forward_renderer_data.transparent_pipeline.layout = pipeline_layout;

    PipelineBuilder builder;
    builder.layout = pipeline_layout;
    builder.set_shaders(mesh_vert_shader, mesh_frag_shader);
    builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder.set_rasterizer_polygon_mode(VK_POLYGON_MODE_FILL);
    // builder.set_rasterizer_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    builder.set_rasterizer_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.set_multisampling_none();
    // builder.enable_blending_additive_for_single_attachment();
    builder.disable_blending();
    // builder.enable_depth_test(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    builder.enable_depth_test(true, VK_COMPARE_OP_LESS);
    // builder.disable_depth_test();

    builder.set_single_color_attachment_format(draw_image.format);
    builder.set_depth_format(depth_image.format);

    forward_renderer_data.opaque_pipeline.pipeline = builder.build_pipeline(device, "Opaque Pipeline");

    builder.enable_blending_additive_for_single_attachment();

    forward_renderer_data.transparent_pipeline.pipeline = builder.build_pipeline(device, "Transparent Pipeline");

    vkDestroyShaderModule(device, mesh_vert_shader, nullptr);
    vkDestroyShaderModule(device, mesh_frag_shader, nullptr);

}

void GLTFHDRMaterial::build_deferred_renderer_pipelines(VkDevice device,
                                                        VkDescriptorSetLayout light_data_descriptor_layout,
                                                        VkDescriptorSetLayout scene_data_descriptor_layout,
                                                        VkDescriptorSetLayout shadow_map_descriptor_layout,
                                                        VkDescriptorSetLayout g_buffer_descriptor_layout,
                                                        std::vector<AllocatedImage>& g_buffers,
                                                        AllocatedImage& depth_g_buffer,
                                                        AllocatedImage& draw_image) {

    if(material_layout == VK_NULL_HANDLE) {
        fmt::print("Shared resources not built before attempting to build deferred renderer pipelines");
        return;
    }

    // GEOMETRY PIPELINE
    VkShaderModule deferred_geometry_vert_shader;
    if(!vk_file::load_shader_module("../shaders/geometry_pass.vert.spv", device, &deferred_geometry_vert_shader)) {
        fmt::print("Error loading deferred geometry vert shader\n");
    }

    VkShaderModule deferred_geometry_frag_shader;
    if(!vk_file::load_shader_module("../shaders/geometry_pass.frag.spv", device, &deferred_geometry_frag_shader)) {
        fmt::print("Error loading deferred geometry frag shader\n");
    }

    VkPushConstantRange geometry_push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(GPUDrawPushConstants),
    };

    std::vector<VkPushConstantRange> geometry_push_constant_ranges { // also used in lighting stage
            geometry_push_constant_range
    };

    std::vector<VkDescriptorSetLayout> geometry_descriptor_set_layouts {
            scene_data_descriptor_layout,
            material_layout
    };

    VkPipelineLayout geometry_pipeline_layout;
    VkPipelineLayoutCreateInfo geometry_pipeline_layout_info = vk_init::get_pipeline_layout_create_info(geometry_descriptor_set_layouts, geometry_push_constant_ranges);
    VK_CHECK(vkCreatePipelineLayout(device, &geometry_pipeline_layout_info, nullptr, &geometry_pipeline_layout));

    deferred_renderer_data.geometry_pipeline.layout = geometry_pipeline_layout;

    PipelineBuilder geometry_pipeline_builder;
    geometry_pipeline_builder.layout = geometry_pipeline_layout;
    geometry_pipeline_builder.set_shaders(deferred_geometry_vert_shader, deferred_geometry_frag_shader);
    geometry_pipeline_builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    geometry_pipeline_builder.set_rasterizer_polygon_mode(VK_POLYGON_MODE_FILL);
    geometry_pipeline_builder.set_rasterizer_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    geometry_pipeline_builder.set_multisampling_none();
    // geometry_pipeline_builder.disable_blending();
    geometry_pipeline_builder.enable_depth_test(true, VK_COMPARE_OP_LESS);
    // geometry_pipeline_builder.disable_depth_test();

    std::vector<VkFormat> color_attachment_formats; // (g-buffer attachment formats)
    std::vector<PipelineBuilder::BlendOptions> blend_options;
    for(AllocatedImage& g_buffer : g_buffers) {
        color_attachment_formats.push_back(g_buffer.format);
        blend_options.push_back(PipelineBuilder::BlendOptions::NoBlend);
    }

    geometry_pipeline_builder.set_multiple_color_attachment_formats_and_blending_styles(color_attachment_formats, blend_options);
    geometry_pipeline_builder.set_depth_format(depth_g_buffer.format);

    deferred_renderer_data.geometry_pipeline.pipeline = geometry_pipeline_builder.build_pipeline(device, "Deferred Geometry Pipeline");

    vkDestroyShaderModule(device, deferred_geometry_vert_shader, nullptr);
    vkDestroyShaderModule(device, deferred_geometry_frag_shader, nullptr);

    // LIGHT PIPELINE
    VkShaderModule light_vert_shader;
    if(!vk_file::load_shader_module("../shaders/lighting_pass.vert.spv", device, &light_vert_shader)) {
        fmt::print("Error loading deferred light vert shader\n");
    }

    VkShaderModule light_frag_shader;
    if(!vk_file::load_shader_module("../shaders/lighting_pass.frag.spv", device, &light_frag_shader)) {
        fmt::print("Error loading deferred light frag shader\n");
    }

    std::vector<VkDescriptorSetLayout> light_descriptor_set_layouts {
            scene_data_descriptor_layout,
            light_data_descriptor_layout,
            g_buffer_descriptor_layout
    };

    VkPushConstantRange lighting_push_constant_range = { // also used in lighting stage
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(VkDeviceAddress),
    };

    std::vector<VkPushConstantRange> lighting_push_constant_ranges { // also used in lighting stage
            lighting_push_constant_range
    };

    VkPipelineLayout light_pipeline_layout;
    VkPipelineLayoutCreateInfo pipeline_layout_info = vk_init::get_pipeline_layout_create_info(light_descriptor_set_layouts, lighting_push_constant_ranges);
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &light_pipeline_layout));

    deferred_renderer_data.light_pipeline.layout = light_pipeline_layout;

    PipelineBuilder light_pipeline_builder;
    light_pipeline_builder.layout = light_pipeline_layout;
    light_pipeline_builder.set_shaders(light_vert_shader, light_frag_shader);
    light_pipeline_builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    light_pipeline_builder.set_rasterizer_polygon_mode(VK_POLYGON_MODE_FILL);
    light_pipeline_builder.set_rasterizer_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    light_pipeline_builder.set_multisampling_none();
    light_pipeline_builder.disable_blending();
    light_pipeline_builder.disable_depth_test();

    light_pipeline_builder.set_single_color_attachment_format(draw_image.format);
    // light_pipeline_builder.set_depth_format(VK_FORMAT_UNDEFINED); // todo -- is this correct?

    deferred_renderer_data.light_pipeline.pipeline = light_pipeline_builder.build_pipeline(device, "Deferred Lighting Pipeline");

    vkDestroyShaderModule(device, light_vert_shader, nullptr);
    vkDestroyShaderModule(device, light_frag_shader, nullptr);
}


MaterialInstance GLTFHDRMaterial::write_material(VkDevice device, MaterialPassType pass_type,
                                                 const GLTFHDRMaterial::MaterialResources& resources,
                                                 DescriptorAllocatorGrowable& descriptor_allocator) {

    MaterialInstance mat_data;
    mat_data.pass_type = pass_type;
    if(pass_type == MaterialPassType::Transparent) {
        mat_data.forward_rendering_pipeline = &forward_renderer_data.transparent_pipeline;
        mat_data.deferred_rendering_geometry_pipeline = &deferred_renderer_data.geometry_pipeline;
    } else {
        mat_data.forward_rendering_pipeline = &forward_renderer_data.opaque_pipeline;
        mat_data.deferred_rendering_geometry_pipeline = &deferred_renderer_data.geometry_pipeline;
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

void GLTFHDRMaterial::clear_resources(VkDevice device) {
    // FORWARD
    vkDestroyPipelineLayout(device, forward_renderer_data.opaque_pipeline.layout, nullptr);
    vkDestroyPipeline(device, forward_renderer_data.opaque_pipeline.pipeline, nullptr);

    vkDestroyPipelineLayout(device, forward_renderer_data.transparent_pipeline.layout, nullptr);
    vkDestroyPipeline(device, forward_renderer_data.transparent_pipeline.pipeline, nullptr);

    // DEFERRED
    vkDestroyPipelineLayout(device, deferred_renderer_data.geometry_pipeline.layout, nullptr);
    vkDestroyPipeline(device, deferred_renderer_data.geometry_pipeline.pipeline, nullptr);

    vkDestroyPipelineLayout(device, deferred_renderer_data.light_pipeline.layout, nullptr);
    vkDestroyPipeline(device, deferred_renderer_data.light_pipeline.pipeline, nullptr);

    // SHARED
    vkDestroyDescriptorSetLayout(device, material_layout, nullptr);
}


