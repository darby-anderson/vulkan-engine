//
// Created by darby on 2/9/2025.
//

#include "ForwardRenderer.hpp"
#include "VulkanInitUtility.hpp"
#include "VulkanDebugUtility.hpp"
#include "DescriptorLayoutBuilder.hpp"
#include "VulkanImageUtility.hpp"
#include "GLTFHDRMaterial.hpp"
#include <chrono>

// What can I move to an initialization?
//

void ForwardRenderer::init(VkDevice device,
                           VmaAllocator allocator,
                           AllocatedImage& draw_image,
                           std::shared_ptr<ShadowPipeline>& shadow_pipeline,
                           VkDescriptorSetLayout scene_descriptor_set_layout,
                           VkDescriptorSetLayout shadow_map_descriptor_set_layout,
                           VkDescriptorSetLayout light_source_descriptor_set_layout,
                           GLTFHDRMaterial& pipeline_builder) {

    this->device = device;
    this->allocator = allocator;
    this->draw_image = draw_image;
    this->shadow_pipeline = shadow_pipeline;
    this->scene_descriptor_set_layout = scene_descriptor_set_layout;
    this->shadow_map_descriptor_set_layout = shadow_map_descriptor_set_layout;

    // Create renderer's descriptor allocator
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
            // dedicated 100% of the pool to storage image descriptor types
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
    };

    renderer_descriptor_allocator.init_allocator(device, 10, sizes);

    // Draw image descriptor set creation
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        draw_image_descriptor_layout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    draw_image_descriptor_set = renderer_descriptor_allocator.allocate(device, draw_image_descriptor_layout, nullptr);

    DescriptorWriter descriptor_writer;
    descriptor_writer.write_image(0, draw_image.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptor_writer.update_set(device, draw_image_descriptor_set);

    // & captures all variables by reference
    renderer_deletion_queue.push_function([&]() {
        renderer_descriptor_allocator.destroy_descriptor_pools(device);
        vkDestroyDescriptorSetLayout(device, draw_image_descriptor_layout, nullptr);
    });

    // Create depth image
    depth_image.extent = draw_image.extent;
    depth_image.format = VK_FORMAT_D32_SFLOAT;

    VkImageUsageFlags depth_image_usage_flags = {};
    depth_image_usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo depth_image_alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkImageCreateInfo depth_image_create_info = vk_init::get_image_create_info(depth_image.format, depth_image_usage_flags, depth_image.extent);
    VK_CHECK(vmaCreateImage(allocator, &depth_image_create_info, &depth_image_alloc_info, &depth_image.image, &depth_image.allocation, nullptr));
    vk_debug::name_resource<VkImage>(device, VK_OBJECT_TYPE_IMAGE, depth_image.image, "Depth Image");

    VkImageViewCreateInfo depth_image_view_create_info = vk_init::get_image_view_create_info(depth_image.format, depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(device, &depth_image_view_create_info, nullptr, &depth_image.view));
    vk_debug::name_resource<VkImageView>(device, VK_OBJECT_TYPE_IMAGE_VIEW, depth_image.view, "Depth Image View");


    // Tone Mapping Pipeline
    std::vector<VkDescriptorSetLayout> tone_mapping_descriptor_layouts = {
            draw_image_descriptor_layout
    };

    VkPushConstantRange compute_push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(ToneMappingComputePushConstants),
    };

    std::vector<VkPushConstantRange> tone_mapping_push_constant_ranges = {
            compute_push_constant_range
    };

    tone_mapping_pipeline.init(
            device,
            "../shaders/tone_mapping.comp.spv",
            tone_mapping_descriptor_layouts,
            tone_mapping_push_constant_ranges,
            renderer_deletion_queue
    );

    // Create pipelines
    pipeline_builder.build_forward_renderer_pipelines(device,
                                                      light_source_descriptor_set_layout,
                                                      scene_descriptor_set_layout,
                                                      shadow_map_descriptor_set_layout,
                                                      draw_image,
                                                      depth_image);

}

void ForwardRenderer::draw(VkCommandBuffer cmd,
                           DescriptorAllocatorGrowable& frame_descriptor_allocator,
                           DeletionQueue& frame_deletion_queue,
                           AllocatedImage& shadow_map,
                           VkSampler shadow_map_sampler,
                           GPUSceneData& current_scene_data,
                           EngineStats& engine_stats,
                           DrawContext& draw_context,
                           VkDescriptorSet* light_data_descriptor_set) {


    // transition from undefined (image either created or in unknown state), to general (for the clear operation)
    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    clear_draw_image(cmd);

    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk_image::transition_image_layout(cmd, depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_geometry_into_draw_image(cmd, frame_descriptor_allocator, frame_deletion_queue, shadow_map, shadow_map_sampler,
                              current_scene_data, engine_stats, draw_context, light_data_descriptor_set);

    // TEST
//    VkRenderingAttachmentInfo color_attachment = vk_init::get_color_attachment_info(draw_image.view, nullptr);
//    std::vector<VkRenderingAttachmentInfo> color_attachment_infos = {
//            color_attachment
//    };
//    VkRenderingAttachmentInfo depth_attachment_info = vk_init::get_depth_attachment_info(depth_image.view);
//
//    VkExtent2D render_extent = {
//            .width = draw_image.extent.width,
//            .height = draw_image.extent.height
//    };
//
//    VkRenderingInfo render_info = vk_init::get_rendering_info(render_extent, color_attachment_infos, nullptr);
//
//    vkCmdBeginRendering(cmd, &render_info);
//
//    vkCmdEndRendering(cmd);
    // END TEST


    // HDR Mapping using compute shaders
    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    tone_map_draw_image(cmd);

    // Make draw image a valid source for being transferred into the swapchain
    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}


void ForwardRenderer::clear_draw_image(VkCommandBuffer cmd) {
    VkClearColorValue clear_value;
    clear_value = { { 0.0f, 0.0f, 1.0f, 1.0f } };

    VkImageSubresourceRange clear_range = vk_init::get_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);

    /*
        ComputeEffect& current_effect = compute_effects[current_compute_effect];

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, current_effect.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, current_effect.layout, 0, 1, &draw_image_descriptors, 0, nullptr);

        vkCmdPushConstants(cmd, current_effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BackgroundComputePushConstants), &current_effect.data);

        // divide work into 16 parts for the 16 workgroups
        vkCmdDispatch(cmd, std::ceil(draw_image.extent.width / 16.0), std::ceil(draw_image.extent.height / 16.0), 1);
    */
}


void ForwardRenderer::draw_geometry_into_draw_image(VkCommandBuffer cmd,
                                                    DescriptorAllocatorGrowable& frame_descriptor_allocator,
                                                    DeletionQueue& frame_deletion_queue,
                                                    AllocatedImage& shadow_map,
                                                    VkSampler shadow_map_sampler,
                                                    GPUSceneData& current_scene_data,
                                                    EngineStats& engine_stats,
                                                    DrawContext& draw_context,
                                                    VkDescriptorSet* light_data_descriptor_set) {

    engine_stats.draw_call_count = 0;
    engine_stats.triangle_count = 0;
    auto draw_geometry_start = std::chrono::system_clock::now();

    VkRenderingAttachmentInfo color_attachment = vk_init::get_color_attachment_info(draw_image.view, nullptr);
    std::vector<VkRenderingAttachmentInfo> color_attachment_infos = {
            color_attachment
    };
    VkRenderingAttachmentInfo depth_attachment_info = vk_init::get_depth_attachment_info(depth_image.view);

    VkExtent2D render_extent = {
            .width = draw_image.extent.width,
            .height = draw_image.extent.height
    };

    VkRenderingInfo render_info = vk_init::get_rendering_info(render_extent, color_attachment_infos, &depth_attachment_info);

    vkCmdBeginRendering(cmd, &render_info);

    VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(render_extent.width),
            .height = static_cast<float>(render_extent.height),
            .minDepth = 0.f,
            .maxDepth = 1.f
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {
            .offset = {
                    .x = 0,
                    .y = 0
            },
            .extent = {
                    .width = render_extent.width,
                    .height = render_extent.height,
            }
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDescriptorSet shadow_map_descriptor_set = shadow_pipeline->create_frame_shadow_map_descriptor_set(device,
                                                                                                        shadow_map,
                                                                                                        shadow_map_sampler,
                                                                                                        frame_descriptor_allocator,
                                                                                                        shadow_map_descriptor_set_layout);

    // Create the GPU scene data buffer for this frame
    // This handles the data-race which may occur if we updated a uniform buffer being read-from by inflight shader executions
    Buffer gpu_scene_data_buffer;
    gpu_scene_data_buffer.init(allocator, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    frame_deletion_queue.push_function([=, this](){
        gpu_scene_data_buffer.destroy_buffer();
    });

    // Get the memory handle mapped to the buffer's allocation
    GPUSceneData* scene_uniform_data = (GPUSceneData*)gpu_scene_data_buffer.info.pMappedData;
    *scene_uniform_data = current_scene_data;

    // a descriptor is a handle to an object like an image or buffer
    // a descriptor set is a bundle of those handles
    VkDescriptorSet global_descriptor_set = frame_descriptor_allocator.allocate(device, scene_descriptor_set_layout, nullptr);

    // write the buffer's handle into the descriptor set
    // then update the descriptor set to use the correct buffer handle
    DescriptorWriter writer;
    writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(device, global_descriptor_set);

    for(const RenderObject& draw : draw_context.opaque_surfaces) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->forward_rendering_pipeline->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->forward_rendering_pipeline->layout, 0, 1, &global_descriptor_set, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->forward_rendering_pipeline->layout, 1, 1, light_data_descriptor_set, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->forward_rendering_pipeline->layout, 2, 1, &shadow_map_descriptor_set, 0, nullptr);

        // Tell the GPU which material-specific set of variables in memory we want to currently use
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->forward_rendering_pipeline->layout, 3, 1, &draw.material->material_set, 0, nullptr);

        vkCmdBindIndexBuffer(cmd, draw.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        GPUDrawPushConstants push_constants = {
                .world_matrix = draw.transform,
                .vertex_buffer_address = draw.vertex_buffer_address
        };
        vkCmdPushConstants(cmd, draw.material->forward_rendering_pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

        vkCmdDrawIndexed(cmd, draw.index_count, 1, draw.first_index, 0, 0);

        engine_stats.draw_call_count++;
        engine_stats.triangle_count += draw.index_count / 3;
    }

    vkCmdEndRendering(cmd);

    auto draw_geometry_end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds >(draw_geometry_end - draw_geometry_start);
    engine_stats.mesh_draw_time = elapsed.count() / 1000.f;

}

void ForwardRenderer::tone_map_draw_image(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tone_mapping_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tone_mapping_pipeline.layout, 0, 1, &draw_image_descriptor_set, 0, nullptr);
    vkCmdPushConstants(cmd, tone_mapping_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ToneMappingComputePushConstants), &tone_mapping_data);
    vkCmdDispatch(cmd, std::ceil(draw_image.extent.width / 16.0), std::ceil(draw_image.extent.height / 16.0), 1);
}

void ForwardRenderer::destroy() {
    depth_image.destroy(device);

    renderer_deletion_queue.flush();
}
