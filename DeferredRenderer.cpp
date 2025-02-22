//
// Created by darby on 2/10/2025.
//

#include "DeferredRenderer.hpp"
#include "VulkanDebugUtility.hpp"
#include "VulkanInitUtility.hpp"
#include "DescriptorLayoutBuilder.hpp"
#include "VulkanGeneralUtility.hpp"
#include "VulkanImageUtility.hpp"
#include <chrono>

void DeferredRenderer::create_g_buffer(AllocatedImage& g_buffer, VkExtent3D extent, const std::string& name) {
    g_buffer.extent = draw_image.extent;
    g_buffer.format = VK_FORMAT_R8G8B8A8_UNORM;

    VkImageUsageFlags g_buffer_usage_flags = {};
    g_buffer_usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    g_buffer_usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    g_buffer_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo depth_image_alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkImageCreateInfo g_buffer_create_info = vk_init::get_image_create_info(g_buffer.format, g_buffer_usage_flags, g_buffer.extent);
    VK_CHECK(vmaCreateImage(allocator, &g_buffer_create_info, &depth_image_alloc_info, &g_buffer.image, &g_buffer.allocation, nullptr));
    vk_debug::name_resource<VkImage>(device, VK_OBJECT_TYPE_IMAGE, g_buffer.image, (name + " image").c_str());

    VkImageViewCreateInfo depth_image_view_create_info = vk_init::get_image_view_create_info(g_buffer.format, g_buffer.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device, &depth_image_view_create_info, nullptr, &g_buffer.view));
    vk_debug::name_resource<VkImageView>(device, VK_OBJECT_TYPE_IMAGE_VIEW, g_buffer.view, (name + "  view").c_str());

}

void DeferredRenderer::init(VkDevice device, VmaAllocator allocator, AllocatedImage& draw_image,
                            std::shared_ptr<ShadowPipeline>& shadow_pipeline,
                            VkDescriptorSetLayout scene_descriptor_set_layout,
                            VkDescriptorSetLayout shadow_map_descriptor_set_layout,
                            VkDescriptorSetLayout light_source_descriptor_set_layout,
                            GLTFHDRMaterial& hdr_mat_pipeline_builder,
                            ImmediateSubmitCommandBuffer& immediate_submit_command_buffer) {

    this->device = device;
    this->allocator = allocator;
    this->draw_image = draw_image;
    this->shadow_pipeline = shadow_pipeline;
    this->scene_descriptor_set_layout = scene_descriptor_set_layout;
    this->shadow_map_descriptor_set_layout = shadow_map_descriptor_set_layout;

    VkExtent3D g_buffer_extent = {
            .width = draw_image.extent.width,
            .height = draw_image.extent.height,
            .depth = draw_image.extent.depth,
    };

    // Create Depth G-Buffer {
    {
        depth_g_buffer.extent = g_buffer_extent;
        depth_g_buffer.format = VK_FORMAT_D32_SFLOAT;

        VkImageUsageFlags depth_image_usage_flags = {};
        depth_image_usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depth_image_usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

        VmaAllocationCreateInfo depth_g_buffer_alloc_info = {
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .requiredFlags = VkMemoryPropertyFlags (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        VkImageCreateInfo depth_image_create_info = vk_init::get_image_create_info(depth_g_buffer.format, depth_image_usage_flags, depth_g_buffer.extent);
        VK_CHECK(vmaCreateImage(allocator, &depth_image_create_info, &depth_g_buffer_alloc_info, &depth_g_buffer.image, &depth_g_buffer.allocation, nullptr));
        vk_debug::name_resource<VkImage>(device, VK_OBJECT_TYPE_IMAGE, depth_g_buffer.image, "Depth G-Buffer Image");

        VkImageViewCreateInfo depth_image_view_create_info = vk_init::get_image_view_create_info(depth_g_buffer.format, depth_g_buffer.image, VK_IMAGE_ASPECT_DEPTH_BIT);
        VK_CHECK(vkCreateImageView(device, &depth_image_view_create_info, nullptr, &depth_g_buffer.view));
        vk_debug::name_resource<VkImageView>(device, VK_OBJECT_TYPE_IMAGE_VIEW, depth_g_buffer.view, "Depth G-Buffer View");
    }

    // Create Other G-Buffers
    create_g_buffer(world_normal_g_buffer, g_buffer_extent, "World Normal G-Buffer");
    create_g_buffer(albedo_g_buffer, g_buffer_extent, "Albedo G-Buffer");

    // renderer-specific descriptor set pool and layouts

    // Create renderer's descriptor allocator
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
            // dedicated 100% of the pool to storage image descriptor types
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
    };

    renderer_descriptor_allocator.init_allocator(device, 10, sizes);

    // Great explanation of drawing a single triangle here (https://github.com/lwjglgamedev/vulkanbook/blob/master/bookcontents/chapter-10/chapter-10.md)
    std::array<DeferredLightingTriangleVertex, 3> tri_vertices;

    // vulkan NDC coords are -1 to 1, so draw a triangle just large enough to have an interior quad containing that area
    tri_vertices[0].pos = {-1.0, 1.0, 0.0};
    tri_vertices[1].pos = { 3.0, 1.0, 0.0};
    tri_vertices[2].pos = {-1.0,-3.0, 0.0};

    // we are describing an interior quad of the triangle using these texcoords
    tri_vertices[0].tex_coord = {0.0,0.0};
    tri_vertices[1].tex_coord = {2.0,0.0};
    tri_vertices[2].tex_coord = {0.0,2.0};

    std::array<uint32_t,3> tri_indices;

    tri_indices[0] = 0;
    tri_indices[1] = 1;
    tri_indices[2] = 2;

    lighting_pass_triangle_buffer = vk_util::upload_mesh<DeferredLightingTriangleVertex>(tri_indices, tri_vertices, allocator, device, immediate_submit_command_buffer, "deferred pass lighting triangle");

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // depth buffer
        builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // world normal buffer
        builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // albedo buffer
        lighting_pass_lighting_descriptor_set_layout = builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    std::vector<AllocatedImage> g_buffers = {
            world_normal_g_buffer,
            albedo_g_buffer
    };

    hdr_mat_pipeline_builder.build_deferred_renderer_pipelines(device,
                                                               light_source_descriptor_set_layout,
                                                               scene_descriptor_set_layout,
                                                               shadow_map_descriptor_set_layout,
                                                               lighting_pass_lighting_descriptor_set_layout,
                                                               g_buffers,
                                                               depth_g_buffer,
                                                               draw_image);

    deferred_renderer_data = hdr_mat_pipeline_builder.deferred_renderer_data;

}

void DeferredRenderer::draw(VkCommandBuffer cmd, VkCommandBuffer cmd2,
                            DescriptorAllocatorGrowable& frame_descriptor_allocator,
                            DeletionQueue& frame_deletion_queue, AllocatedImage& shadow_map,
                            VkSampler shadow_map_sampler, VkSampler g_buffer_sampler, GPUSceneData& current_scene_data,
                            EngineStats& engine_stats, DrawContext& draw_context,
                            VkDescriptorSet* p_light_data_descriptor_set) {

//    {
//        VkRenderingAttachmentInfo normal_g_buffer_attachment = vk_init::get_color_attachment_info(world_normal_g_buffer.view, nullptr);
//        VkRenderingAttachmentInfo albedo_g_buffer_attachment = vk_init::get_color_attachment_info(albedo_g_buffer.view, nullptr);
//        std::vector<VkRenderingAttachmentInfo> color_attachment_infos = {
//                normal_g_buffer_attachment,
//                albedo_g_buffer_attachment
//        };
//        VkRenderingAttachmentInfo depth_attachment_info = vk_init::get_depth_attachment_info(depth_g_buffer.view);
//
//        VkExtent2D render_extent = {
//                .width = draw_image.extent.width,
//                .height = draw_image.extent.height
//        };
//
//        // ORIGINAL!!! VkRenderingInfo render_info = vk_init::get_rendering_info(render_extent, color_attachment_infos, &depth_attachment_info);
//        VkRenderingInfo render_info = vk_init::get_rendering_info(render_extent, color_attachment_infos, &depth_attachment_info);
//
//        vkCmdBeginRendering(cmd, &render_info);
//
//        vkCmdEndRendering(cmd);
//    }
//
//    vkCmdPipelineBarrier(
//            cmd,
//            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//            0, 0, nullptr, 0, nullptr, 0, nullptr
//    );

    vk_image::transition_image_layout(cmd, world_normal_g_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk_image::transition_image_layout(cmd, albedo_g_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk_image::transition_image_layout(cmd, depth_g_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // 1
    {
        VkRenderingAttachmentInfo color_1_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = albedo_g_buffer.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        VkRenderingAttachmentInfo color_2_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = world_normal_g_buffer.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        VkRenderingAttachmentInfo color_attachments[2] = {
                color_1_attachment,
                color_2_attachment
        };

        VkRenderingAttachmentInfo depth_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = depth_g_buffer.view,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        VkExtent2D render_extent = {
                .width = draw_image.extent.width,
                .height = draw_image.extent.height
        };

        VkRenderingInfo render_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .pNext = nullptr,
                .renderArea = VkRect2D { VkOffset2D{0, 0}, render_extent },
                .layerCount = 1,
                .colorAttachmentCount = 2,
                .pColorAttachments = &color_attachments[0],
                .pDepthAttachment = &depth_attachment,
                .pStencilAttachment = nullptr
        };

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

        // SET UP DESCRIPTOR SETS
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

        VkDescriptorSet scene_data_descriptor_set = frame_descriptor_allocator.allocate(device, scene_descriptor_set_layout, nullptr);

        // write the buffer's handle into the descriptor set
        // then update the descriptor set to use the correct buffer handle
        DescriptorWriter writer;
        writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.update_set(device, scene_data_descriptor_set);

        for(const RenderObject& draw : draw_context.opaque_surfaces) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->deferred_rendering_geometry_pipeline->pipeline);

            // BIND SCENE DATA BUFFER - set 0
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->deferred_rendering_geometry_pipeline->layout, 0, 1, &scene_data_descriptor_set, 0, nullptr);

            // BIND MATERIAL DATA BUFFER - set 1
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->deferred_rendering_geometry_pipeline->layout, 1, 1, &draw.material->material_set, 0, nullptr);

            // BIND INDEX BUFFER
            vkCmdBindIndexBuffer(cmd, draw.index_buffer, 0, VK_INDEX_TYPE_UINT32);

            // PUSH WORLD MATRIX AND VERTEX BUFFER ADDRESS
            GPUDrawPushConstants push_constants = {
                .world_matrix = draw.transform,
                .vertex_buffer_address = draw.vertex_buffer_address
            };
            vkCmdPushConstants(cmd, draw.material->deferred_rendering_geometry_pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

            vkCmdDrawIndexed(cmd, draw.index_count, 1, draw.first_index, 0, 0);
        }

        vkCmdEndRendering(cmd);
    }

    // 2
    {
        VkRenderingAttachmentInfo color_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = draw_image.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        VkExtent2D render_extent = {
                .width = draw_image.extent.width,
                .height = draw_image.extent.height
        };

        VkRenderingInfo render_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .pNext = nullptr,
                .renderArea = VkRect2D { VkOffset2D{0, 0}, render_extent },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_attachment,
                .pDepthAttachment = nullptr,
                .pStencilAttachment = nullptr
        };

        vkCmdBeginRendering(cmd, &render_info);
        vkCmdEndRendering(cmd);
    }

    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // transition from undefined (image either created or in unknown state), to general (for the clear operation)
//    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
//    vk_image::transition_image_layout(cmd, world_normal_g_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
//    vk_image::transition_image_layout(cmd, albedo_g_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
//
//    clear_image_resources(cmd);
//
//    vk_image::transition_image_layout(cmd, world_normal_g_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//    vk_image::transition_image_layout(cmd, albedo_g_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//    vk_image::transition_image_layout(cmd, depth_g_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
//
////    draw_geometry_into_g_buffers(cmd, frame_descriptor_allocator, frame_deletion_queue, current_scene_data, engine_stats, draw_context);
//
//    vk_image::transition_image_layout(cmd, world_normal_g_buffer.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//    vk_image::transition_image_layout(cmd, albedo_g_buffer.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//    vk_image::transition_image_layout_specify_aspect(cmd, depth_g_buffer.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
//    // commented from clear resources above ::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//
////    draw_lighting_pass(cmd, frame_descriptor_allocator, p_light_data_descriptor_set, g_buffer_sampler,
////                       frame_deletion_queue, current_scene_data, engine_stats);
//
//    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

void DeferredRenderer::clear_image_resources(VkCommandBuffer cmd) {
    VkClearColorValue clear_value;
    clear_value = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    VkImageSubresourceRange clear_range = vk_init::get_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);
    vkCmdClearColorImage(cmd, world_normal_g_buffer.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);
    vkCmdClearColorImage(cmd, albedo_g_buffer.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);

    // vkCmdClearColorImage(cmd, depth_g_buffer.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);
}

void DeferredRenderer::draw_geometry_into_g_buffers(VkCommandBuffer cmd,
                                                    DescriptorAllocatorGrowable& frame_descriptor_allocator,
                                                    DeletionQueue& frame_deletion_queue,
                                                    GPUSceneData& current_scene_data,
                                                    EngineStats& engine_stats,
                                                    DrawContext& draw_context) {

    VkRenderingAttachmentInfo normal_g_buffer_attachment = vk_init::get_color_attachment_info(world_normal_g_buffer.view, nullptr);
    VkRenderingAttachmentInfo albedo_g_buffer_attachment = vk_init::get_color_attachment_info(albedo_g_buffer.view, nullptr);
    std::vector<VkRenderingAttachmentInfo> color_attachment_infos = {
            normal_g_buffer_attachment,
            albedo_g_buffer_attachment
    };
    VkRenderingAttachmentInfo depth_attachment_info = vk_init::get_depth_attachment_info(depth_g_buffer.view);


// 1.)
//    VkExtent2D fake_render_extent = {
//            .width = draw_image.extent.width,
//            .height = draw_image.extent.height
//    };
//
//    VkRenderingInfo info_f1 = {
//            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
//            .pNext = nullptr,
//            .renderArea = VkRect2D { VkOffset2D{0, 0}, fake_render_extent },
//            .layerCount = 1,
//            .colorAttachmentCount = 1,
//            .pColorAttachments = &normal_g_buffer_attachment,
//            .pDepthAttachment = nullptr,
//            .pStencilAttachment = nullptr
//    };
//
//    vkCmdBeginRendering(cmd, &info_f1);
//    vkCmdEndRendering(cmd);
//
//    VkRenderingInfo info_f2 = {
//            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
//            .pNext = nullptr,
//            .renderArea = VkRect2D { VkOffset2D{0, 0}, fake_render_extent },
//            .layerCount = 1,
//            .colorAttachmentCount = 0,
//            .pColorAttachments = nullptr,
//            .pDepthAttachment = &depth_attachment_info,
//            .pStencilAttachment = nullptr
//    };
//
//    vkCmdBeginRendering(cmd, &info_f2);
//    vkCmdEndRendering(cmd);


    engine_stats.draw_call_count = 0;
    engine_stats.triangle_count = 0;
    auto draw_geometry_start = std::chrono::system_clock::now();

    // DYNAMIC RENDERING SETUP
//    VkRenderingAttachmentInfo normal_g_buffer_attachment = vk_init::get_color_attachment_info(world_normal_g_buffer.view, nullptr);
//    VkRenderingAttachmentInfo albedo_g_buffer_attachment = vk_init::get_color_attachment_info(albedo_g_buffer.view, nullptr);
//    std::vector<VkRenderingAttachmentInfo> color_attachment_infos = {
//            normal_g_buffer_attachment,
//            albedo_g_buffer_attachment
//    };
//    VkRenderingAttachmentInfo depth_attachment_info = vk_init::get_depth_attachment_info(depth_g_buffer.view);

    VkExtent2D render_extent = {
            .width = draw_image.extent.width,
            .height = draw_image.extent.height
    };

    // ORIGINAL!!! VkRenderingInfo render_info = vk_init::get_rendering_info(render_extent, color_attachment_infos, &depth_attachment_info);
    VkRenderingInfo render_info = vk_init::get_rendering_info(render_extent, color_attachment_infos, &depth_attachment_info);

    vkCmdBeginRendering(cmd, &render_info);

    // VIEWPORT/SCISSOR DATA
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

    // SET UP DESCRIPTOR SETS
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

    VkDescriptorSet scene_data_descriptor_set = frame_descriptor_allocator.allocate(device, scene_descriptor_set_layout, nullptr);

    // write the buffer's handle into the descriptor set
    // then update the descriptor set to use the correct buffer handle
    DescriptorWriter writer;
    writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(device, scene_data_descriptor_set);

    for(const RenderObject& draw : draw_context.opaque_surfaces) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->deferred_rendering_geometry_pipeline->pipeline);

        // BIND SCENE DATA BUFFER - set 0
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->deferred_rendering_geometry_pipeline->layout, 0, 1, &scene_data_descriptor_set, 0, nullptr);

        // BIND MATERIAL DATA BUFFER - set 1
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->deferred_rendering_geometry_pipeline->layout, 1, 1, &draw.material->material_set, 0, nullptr);

        // BIND INDEX BUFFER
        vkCmdBindIndexBuffer(cmd, draw.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        // PUSH WORLD MATRIX AND VERTEX BUFFER ADDRESS
        GPUDrawPushConstants push_constants = {
                .world_matrix = draw.transform,
                .vertex_buffer_address = draw.vertex_buffer_address
        };
        vkCmdPushConstants(cmd, draw.material->deferred_rendering_geometry_pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

        vkCmdDrawIndexed(cmd, draw.index_count, 1, draw.first_index, 0, 0);

        engine_stats.draw_call_count++;
        engine_stats.triangle_count += draw.index_count / 3;
    }

//    // Add pipeline barrier so G-Buffers aren't used too soon
//    VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // need g-buffers written, and depth buffer written
//    VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // will need the g-buffers for light-pass' fragment shader
//
//    VkMemoryBarrier memory_barrier = {
//            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
//            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
//            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
//    };
//
//    vkCmdPipelineBarrier(cmd, src_stage_mask, dst_stage_mask, 0, 1, &memory_barrier,
//                         0, nullptr,
//                         0, nullptr);

//    // Transition image layout
//    VkImageMemoryBarrier barrier = {
//            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
//            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//            .image = draw_image.image,
//            .subresourceRange = {
//                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//                    .baseMipLevel = 0,
//                    .levelCount = 1,
//                    .baseArrayLayer = 0,
//                    .layerCount = 1,
//            },
//    };
//    vkCmdPipelineBarrier(
//            cmd,
//            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
//            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//            0,
//            0, nullptr,
//            0, nullptr,
//            1, &barrier
//    );

    vkCmdEndRendering(cmd);

    auto draw_geometry_end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds >(draw_geometry_end - draw_geometry_start);
    engine_stats.mesh_draw_time = elapsed.count() / 1000.f;

// 2.)
//    VkExtent2D fake_render_extent = {
//            .width = draw_image.extent.width,
//            .height = draw_image.extent.height
//    };
//
//    VkRenderingInfo fake_render_info = vk_init::get_rendering_info(fake_render_extent, {}, nullptr);
//
//    vkCmdBeginRendering(cmd, &fake_render_info);
//
//    vkCmdEndRendering(cmd);

}

void DeferredRenderer::draw_lighting_pass(VkCommandBuffer cmd, DescriptorAllocatorGrowable& frame_descriptor_allocator,
                                          VkDescriptorSet* light_data_descriptor_set, VkSampler g_buffer_sampler,
                                          DeletionQueue& frame_deletion_queue, GPUSceneData& current_scene_data,
                                          EngineStats& engine_stats) {

    engine_stats.draw_call_count = 0;
    engine_stats.triangle_count = 0;
    auto draw_lighting_start = std::chrono::system_clock::now();


// Set up attachment
//    VkClearValue clear_value = { .color = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} } };
//    VkRenderingAttachmentInfo fake_color_attachment = {
//            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
//            .imageView = draw_image.view,
//            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Use CLEAR for simplicity
//            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
//            .clearValue = clear_value,
//    };
//
//    VkExtent2D render_extent = { draw_image.extent.width, draw_image.extent.height };
//    VkRenderingInfo info = {
//            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
//            .renderArea = VkRect2D { VkOffset2D{0, 0}, render_extent },
//            .layerCount = 1,
//            .colorAttachmentCount = 1,
//            .pColorAttachments = &fake_color_attachment,
//    };
//    fmt::print("Rendering Info: extent={}x{}, colorAttachmentCount={}, pColorAttachments={}\n",
//               info.renderArea.extent.width, info.renderArea.extent.height,
//               info.colorAttachmentCount, (void*)info.pColorAttachments);
//
//    fmt::print("\n\n\n\n\n\n");
//
//    vkCmdBeginRendering(cmd, &info);

//    if(draw_image.view == VK_NULL_HANDLE) {
//        fmt::print("Error draw_image.view is invalid");
//    }
//
//    VkRenderingAttachmentInfo fake_color_attachment = {
//            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
//            .pNext = nullptr,
//            .imageView = draw_image.view,
//            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
//            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
//    };
//
//    // DYNAMIC RENDERING SETUP
    VkRenderingAttachmentInfo draw_image_attachment = vk_init::get_color_attachment_info(draw_image.view, nullptr);
    std::vector<VkRenderingAttachmentInfo> color_attachment_infos = {
        draw_image_attachment
    };

    VkExtent2D render_extent = {
        .width = draw_image.extent.width,
        .height = draw_image.extent.height
    };

    VkRenderingInfo render_info = vk_init::get_rendering_info(render_extent, color_attachment_infos, nullptr);

//    // uint32_t color_attachment_count = static_cast<uint32_t>(color_attachments.size());
//
//    VkRenderingInfo info = {
//            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
//            .pNext = nullptr,
//            .renderArea = VkRect2D { VkOffset2D{0, 0}, render_extent },
//            .layerCount = 1,
//            .colorAttachmentCount = 1,
//            .pColorAttachments = &fake_color_attachment,
//            .pDepthAttachment = nullptr,
//            .pStencilAttachment = nullptr
//    };
//
    vkCmdBeginRendering(cmd, &render_info);

    // VIEWPORT/SCISSOR DATA
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

    // SET UP DESCRIPTOR SETS
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

    VkDescriptorSet global_descriptor_set = frame_descriptor_allocator.allocate(device, scene_descriptor_set_layout, nullptr);

    // write the buffer's handle into the descriptor set
    // then update the descriptor set to use the correct buffer handle
    DescriptorWriter writer;
    writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(device, global_descriptor_set);

    // G-BUFFER DESCRIPTOR SETS
    VkDescriptorSet g_buffer_descriptor_set = frame_descriptor_allocator.allocate(device, lighting_pass_lighting_descriptor_set_layout, nullptr);

    writer.clear();
    writer.write_image(0, depth_g_buffer.view, g_buffer_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(1, world_normal_g_buffer.view, g_buffer_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2, albedo_g_buffer.view, g_buffer_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update_set(device, g_buffer_descriptor_set);

    // BIND PIPELINE
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_renderer_data.light_pipeline.pipeline);

    // BIND SCENE DESCRIPTOR SET - set 0
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_renderer_data.light_pipeline.layout, 0, 1, &global_descriptor_set, 0, nullptr);

    // BIND LIGHT DESCRIPTOR SET - set 1
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_renderer_data.light_pipeline.layout, 1, 1, light_data_descriptor_set, 0, nullptr);

    // BIND G-BUFFER DESCRIPTOR SET - set 2
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_renderer_data.light_pipeline.layout, 2, 1, &g_buffer_descriptor_set, 0, nullptr);

    // BIND INDEX BUFFER
    vkCmdBindIndexBuffer(cmd, lighting_pass_triangle_buffer.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    // PUSH CONSTANTS - VERTEX BUFFER DATA
    VkDeviceAddress push_constant = lighting_pass_triangle_buffer.vertex_buffer_address;
    vkCmdPushConstants(cmd, deferred_renderer_data.light_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress ), &push_constant);

    vkCmdDrawIndexed(cmd, 3, 1, 0, 0, 0);

    vkCmdEndRendering(cmd);

    auto draw_lighting_end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(draw_lighting_end - draw_lighting_start);
    engine_stats.lighting_draw_time = elapsed.count() / 1000.f;

}


