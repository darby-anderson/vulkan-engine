//
// Created by darby on 11/20/2024.
//
#include <chrono>
#include <thread>

#define VK_USE_PLATFORM_WIN32_KHR
#define VOLK_IMPLEMENTATION
#include "volk.h"

// #define VMA_STATIC_VULKAN_FUNCTIONS 0 // necessary since we load via volk
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define IMGUI_IMPL_VULKAN_USE_VOLK
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include "Engine.hpp"
#include "VulkanInitUtility.hpp"
#include "VulkanFileLoaderUtility.hpp"
#include "VulkanImageUtility.hpp"
#include "DescriptorLayoutBuilder.hpp"
#include "PipelineBuilder.hpp"
#include "DescriptorWriter.hpp"

void Engine::init() {
    frame_number = 0;

    WindowConfiguration window_conf = {
            .width = 1600,
            .height = 1000
    };
    window.init(window_conf);

    vulkan_context.init_vulkan_instance();
    vulkan_context.init_vulkan_surface(window.get_win32_window());
    physical_device.choose_and_init(vulkan_context.instance, vulkan_context.surface);
    device.init(physical_device.physical_device, vulkan_context.surface);
    swapchain.init(device.device, vulkan_context.surface, physical_device.surface_capabilities, physical_device.surface_formats, physical_device.present_modes, window);

    const VmaVulkanFunctions vulkanFunctions = {
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
            .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory = vkAllocateMemory,
            .vkFreeMemory = vkFreeMemory,
            .vkMapMemory = vkMapMemory,
            .vkUnmapMemory = vkUnmapMemory,
            .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory = vkBindBufferMemory,
            .vkBindImageMemory = vkBindImageMemory,
            .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
            .vkCreateBuffer = vkCreateBuffer,
            .vkDestroyBuffer = vkDestroyBuffer,
            .vkCreateImage = vkCreateImage,
            .vkDestroyImage = vkDestroyImage,
            .vkCmdCopyBuffer = vkCmdCopyBuffer,
#if VMA_VULKAN_VERSION >= 1001000
            .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
            .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
            .vkBindBufferMemory2KHR = vkBindBufferMemory2,
            .vkBindImageMemory2KHR = vkBindImageMemory2,
            .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
#endif
#if VMA_VULKAN_VERSION >= 1003000
            .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
            .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements
#endif
    };

    // Create our Vma Allocator
    VmaAllocatorCreateInfo allocator_info = {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physical_device.physical_device,
            .device = device.device,
            .pVulkanFunctions = &vulkanFunctions,
            .instance = vulkan_context.instance,
    };

    vmaCreateAllocator(&allocator_info, &allocator);

    // Init draw and depth image
    init_draw_and_depth_images();

    init_command_structures();

    init_synch_objects();

    init_descriptors();

    init_pipelines();

    init_imgui();

    load_models();

}

void Engine::cleanup() {

    for(auto & frame : frames) {
        vkDestroyCommandPool(device.device, frame.command_pool, nullptr);

        vkDestroyFence(device.device, frame.render_fence, nullptr);
        vkDestroySemaphore(device.device, frame.swapchain_semaphore, nullptr);
        vkDestroySemaphore(device.device, frame.render_semaphore, nullptr);

        frame.deletion_queue.flush();
    }

    engine_deletion_queue.flush();

    window.terminate();
    device.cleanup();
}


void Engine::draw_background(VkCommandBuffer cmd) {
    VkClearColorValue clear_value;
    float flash = std::abs(std::sin(frame_number / 120.f));
    clear_value = { { 0.0f, 0.0f, flash, 1.0f } };

    VkImageSubresourceRange clear_range = vk_init::get_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);

    ComputeEffect& current_effect = compute_effects[current_compute_effect];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, current_effect.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, current_effect.layout, 0, 1, &draw_image_descriptors, 0, nullptr);

    vkCmdPushConstants(cmd, gradient_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &current_effect.data);

    // divide work into 16 parts for the 16 workgroups
    vkCmdDispatch(cmd, std::ceil(draw_image.extent.width / 16.0), std::ceil(draw_image.extent.height / 16.0), 1);
}

void Engine::draw_geometry(VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo color_attachment_info = vk_init::get_color_attachment_info(draw_image.view, nullptr);
    VkRenderingAttachmentInfo depth_attachment_info = vk_init::get_depth_attachment_info(depth_image.view);

    VkRenderingInfo render_info = vk_init::get_rendering_info(swapchain.extent, &color_attachment_info, nullptr);

    vkCmdBeginRendering(cmd, &render_info);

    VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(swapchain.extent.width),
            .height = static_cast<float>(swapchain.extent.height),
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
                    .width = swapchain.extent.width,
                    .height = swapchain.extent.height,
            }
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline);

    glm::mat4 model = glm::scale(glm::vec3(2.f, 2.f, 2.f));

    glm::mat4 view = glm::lookAt(
            glm::vec3(0.f, 0.f, -5.f), // cam pos
            glm::vec3(0.f, 0.f, 0.f), // target pos
            glm::vec3(0.f, 1.f, 0.f) // up vec
    );

    glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)draw_image.extent.width / (float)draw_image.extent.height, 0.1f, 100000.0f);
    projection[1][1] *= -1;

    std::shared_ptr<DrawData> data_to_draw = draw_datas[2];

    GPUDrawPushConstants push_constants = {
            .world_matrix = projection * view * model,
            // .world_matrix = glm::mat4(1.0),
            .vertex_buffer_address = data_to_draw->gpu_mesh_buffers.vertex_buffer_address
    };

    vkCmdPushConstants(cmd, mesh_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdBindIndexBuffer(cmd, data_to_draw->gpu_mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, data_to_draw->submesh_datas[0].indexCount, 1, 0, 0, 0);

    // Create the GPU scene data buffer for this frame
    Buffer gpu_scene_data_buffer;
    gpu_scene_data_buffer.init(allocator, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    get_current_frame().deletion_queue.push_function([=, this](){
        gpu_scene_data_buffer.destroy_buffer();
    });

    // Get the memory handle mapped to the buffer's allocation
    GPUSceneData* scene_uniform_data = (GPUSceneData*)gpu_scene_data_buffer.allocation->GetMappedData();
    *scene_uniform_data = scene_data;

    // a descriptor is a handle to an object like an image or buffer
    // a descriptor set is a bundle of those handles
    VkDescriptorSet global_descriptor_set = get_current_frame().frame_descriptors.allocate(device.device, gpu_scene_descriptor_set_layout, nullptr);

    // write the buffer's handle into the descriptor set
    // then update the descriptor set to use the correct buffer handle
    DescriptorWriter writer;
    writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(device.device, global_descriptor_set);

    vkCmdEndRendering(cmd);
}


void Engine::draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view) {
    VkRenderingAttachmentInfo color_attachment_info = vk_init::get_color_attachment_info(target_image_view, nullptr);
    // VkRenderingInfo rendering_info = vk_init::get_rendering_info(swapchain.extent, &color_attachment_info, nullptr);
    VkRenderingInfo rendering_info = vk_init::get_rendering_info(draw_extent, &color_attachment_info, nullptr);

    vkCmdBeginRendering(cmd, &rendering_info);
    // ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void Engine::draw() {

    // flush global descriptor set
    VK_CHECK(vkWaitForFences(device.device, 1, &get_current_frame().render_fence, true, 1000000000));
    get_current_frame().deletion_queue.flush();
    get_current_frame().frame_descriptors.clear_descriptor_pools(device.device);


    uint32_t swapchain_image_index = swapchain.get_current_swapchain_image_index(get_current_frame().swapchain_semaphore, swapchain_resize_requested);

    // check if swapchain flagged us for a resize
    if(swapchain_resize_requested) {
        fmt::print("Swapchain requested resize\n");
        return;
    }

    VK_CHECK(vkResetFences(device.device, 1, &get_current_frame().render_fence));

    VkImage curr_swapchain_image = swapchain.get_swapchain_image(swapchain_image_index);
    VkImageView curr_swapchain_image_view = swapchain.get_swapchain_image_view(swapchain_image_index);

    draw_extent.width = std::min(swapchain.extent.width, draw_image.extent.width) * render_scale; // accounts for if the swapchain is larger than the draw image
    draw_extent.height = std::min(swapchain.extent.height, draw_image.extent.height) * render_scale;

    VkCommandBuffer cmd = get_current_frame().main_command_buffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo begin_info = vk_init::get_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    // transition from undefined (image was just created), to general (for the clear operation)
    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk_image::transition_image_layout(cmd, depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_geometry(cmd);

    // make draw image a good source, make swapchain image a good destination
    vk_image::transition_image_layout(cmd, draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vk_image::transition_image_layout(cmd, curr_swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // copy draw image into swapchain
    vk_image::copy_image_to_image(cmd, draw_image.image, curr_swapchain_image, draw_extent, swapchain.extent);

    // transition swapchain image to attachment optimal so imgui can draw into it
    vk_image::transition_image_layout(cmd, curr_swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    draw_imgui(cmd, curr_swapchain_image_view);

    // make swapchain image presentable for presentation engine
    vk_image::transition_image_layout(cmd, curr_swapchain_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // submit our cmd buffer to the queue
    VkCommandBufferSubmitInfo cmd_submit_info = vk_init::get_command_buffer_submit_info(cmd);

    // we wait for this semaphore before submitting the command buffer
    VkSemaphoreSubmitInfo wait_semaphore_submit_info = vk_init::get_semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchain_semaphore);
    // we wait for this semaphore before declaring the command buffer done
    VkSemaphoreSubmitInfo signal_semaphore_submit_info = vk_init::get_semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, get_current_frame().render_semaphore);

    VkSubmitInfo2 submit_info = vk_init::get_submit_info(&cmd_submit_info, &wait_semaphore_submit_info, &signal_semaphore_submit_info);
    VK_CHECK(vkQueueSubmit2(device.graphics_queue, 1, &submit_info, get_current_frame().render_fence));

    // present the image to the screen
    VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,

            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &get_current_frame().render_semaphore,

            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain,

            .pImageIndices = &swapchain_image_index
    };

    VkResult present_result = vkQueuePresentKHR(device.graphics_queue, &present_info);

    if(present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
        swapchain_resize_requested = true;
        fmt::print("presentation engine requesting swapchain resize\n");
    } else if(present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR) {
        fmt::print("failure when presenting queue. {}\n", static_cast<uint32_t>(present_result));
    }
}

void Engine::imgui_new_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if(ImGui::Begin("background")) {
        ImGui::SliderFloat("Render Scale: ", &render_scale, 0.3f, 1.f);

        ComputeEffect& selected = compute_effects[current_compute_effect];
        ImGui::Text("Selected effect: ", selected.name);

        ImGui::SliderInt("Effect Index", &current_compute_effect, 0, compute_effects.size() - 1);

        ImGui::InputFloat4("data1", (float*) &selected.data.data1);
        ImGui::InputFloat4("data2", (float*) &selected.data.data2);
        ImGui::InputFloat4("data3", (float*) &selected.data.data3);
        ImGui::InputFloat4("data4", (float*) &selected.data.data4);
    }
    ImGui::End();

    ImGui::Render();
}

void Engine::run() {

    while(!window.should_exit()) {
        window.request_os_messages();

        if(!window.has_focus()) {
            // fmt::print("Sleeping\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        imgui_new_frame();
        draw();

        if(swapchain_resize_requested) {
            fmt::print("resizing swapchain");
            resize_swapchain();
        }

        frame_number++;
    }

}

FrameData& Engine::get_current_frame() {
    return frames[frame_number % FRAME_OVERLAP];
}


/*
 * Create the global descriptor allocator, descriptor layouts, and allocate
 * space for the descriptor sets.
 * Then updates descriptor sets to push changes.
 */
void Engine::init_descriptors() {
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
            // dedicated 100% of the pool to storage image descriptor types
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };

    global_descriptor_allocator.init_allocator(device.device, 10, sizes);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        draw_image_descriptor_layout = builder.build(device.device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    draw_image_descriptors = global_descriptor_allocator.allocate(device.device, draw_image_descriptor_layout, nullptr);

    DescriptorWriter descriptor_writer;
    descriptor_writer.write_image(0, draw_image.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptor_writer.update_set(device.device, draw_image_descriptors);

    // & captures all variables by reference
    engine_deletion_queue.push_function([&]() {
        global_descriptor_allocator.destroy_descriptor_pools(device.device);
        vkDestroyDescriptorSetLayout(device.device, draw_image_descriptor_layout, nullptr);
    });

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        gpu_scene_descriptor_set_layout = builder.build(device.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    for(int i = 0; i < FRAME_OVERLAP; i++) {
        // create descriptor pool for the frame
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        frames[i].frame_descriptors = DescriptorAllocatorGrowable{};
        frames[i].frame_descriptors.init_allocator(device.device, 1000, frame_sizes);

        engine_deletion_queue.push_function([&]() {
           frames[i].frame_descriptors.destroy_descriptor_pools(device.device);
        });
    }

}


void Engine::init_background_pipelines() {

    std::vector<VkDescriptorSetLayout> background_descriptor_layouts = {
            draw_image_descriptor_layout
    };

    VkPushConstantRange compute_push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(ComputePushConstants),
    };

    fmt::print("Compute push constant size: {}\n", sizeof(ComputePushConstants));

    std::vector<VkPushConstantRange> background_push_constant_ranges = {
            compute_push_constant_range
    };

    gradient_pipeline.init(
            device.device,
            "../shaders/interactive_gradient.comp.spv",
            background_descriptor_layouts,
            background_push_constant_ranges,
            engine_deletion_queue);

    ComputePushConstants default_gradient_data = {
            .data1 = glm::vec4(1, 0, 0, 1),
            .data2 = glm::vec4(0, 0, 1, 1)
    };

    ComputeEffect gradient_effect = {
            .name = "gradient effect",
            .pipeline = gradient_pipeline.pipeline,
            .layout = gradient_pipeline.layout,
            .data = default_gradient_data
    };

    sky_pipeline.init(
            device.device,
            "../shaders/sky.comp.spv",
            background_descriptor_layouts,
            background_push_constant_ranges,
            engine_deletion_queue);

    ComputePushConstants default_sky_data = {
            .data1 = glm::vec4(0.1, 0.2, 0.4 ,0.97),
    };

    ComputeEffect sky_effect = {
            .name = "sky",
            .pipeline = sky_pipeline.pipeline,
            .layout = sky_pipeline.layout,
            .data = default_sky_data
    };

    compute_effects.push_back(gradient_effect);
    compute_effects.push_back(sky_effect);

}

void Engine::init_pipelines() {

    // COMPUTE PIPELINES
    init_background_pipelines();

    // GRAPHICS PIPELINES
    // init_triangle_pipeline();
    init_mesh_pipeline();

}

void Engine::init_command_structures() {
    // init command pool and buffers in frames
    VkCommandPoolCreateInfo command_pool_create_info = vk_init::get_command_pool_create_info(device.family_index_graphics.value());

    for(auto & frame : frames) {
        VK_CHECK(vkCreateCommandPool(device.device, &command_pool_create_info, nullptr, &frame.command_pool));

        VkCommandBufferAllocateInfo command_buffer_allocate_info = vk_init::get_command_buffer_allocate_info(frame.command_pool, 1);
        VK_CHECK(vkAllocateCommandBuffers(device.device, &command_buffer_allocate_info, &frame.main_command_buffer));

        engine_deletion_queue.push_function([=, this]() {
            vkDestroyCommandPool(device.device, frame.command_pool, nullptr);
        });
    }

    immediate_submit_command_buffer.init(device.device, device.graphics_queue, device.family_index_graphics.value());

    engine_deletion_queue.push_function([=, this]() {
        immediate_submit_command_buffer.destroy();
    });

}

void Engine::init_synch_objects() {
    // init synchronization objects
    VkFenceCreateInfo fence_create_info = vk_init::get_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphore_create_info = vk_init::get_semaphore_create_info();

    for(auto& frame : frames) {
        VK_CHECK(vkCreateFence(device.device, &fence_create_info, nullptr, &frame.render_fence));

        VK_CHECK(vkCreateSemaphore(device.device, &semaphore_create_info, nullptr, &frame.render_semaphore));
        VK_CHECK(vkCreateSemaphore(device.device, &semaphore_create_info, nullptr, &frame.swapchain_semaphore));

        engine_deletion_queue.push_function([=, this]() {
           vkDestroyFence(device.device, frame.render_fence, nullptr);

           vkDestroySemaphore(device.device, frame.render_semaphore, nullptr);
           vkDestroySemaphore(device.device, frame.swapchain_semaphore, nullptr);
        });
    }
}

void Engine::init_imgui() {
    // Create IMGUI's descriptor pool
    VkDescriptorPoolSize pool_sizes[] =
            { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
          { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
          { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
          { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
          { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
          { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
          { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
          { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
          { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
          { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
          { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 10000,
            .poolSizeCount = (uint32_t)std::size(pool_sizes),
            .pPoolSizes = pool_sizes
    };

    VkDescriptorPool imgui_pool;
    VK_CHECK(vkCreateDescriptorPool(device.device, &pool_create_info, nullptr, &imgui_pool));

    ImGui_ImplVulkan_LoadFunctions([](const char* name, void*) {
        return vkGetInstanceProcAddr(volkGetLoadedInstance(), name);
    });

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window.get_glfw_window(), true);

    ImGui_ImplVulkan_InitInfo imgui_init_info = {
            .Instance = vulkan_context.instance,
            .PhysicalDevice = physical_device.physical_device,
            .Device = device.device,
            .Queue = device.graphics_queue,
            .DescriptorPool = imgui_pool,
            .MinImageCount = 3,
            .ImageCount = 3,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
            .UseDynamicRendering = true,
            // dynamic rendering parameters
            .PipelineRenderingCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &swapchain.surface_format.format
            }
    };

    ImGui_ImplVulkan_Init(&imgui_init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    engine_deletion_queue.push_function([=, this]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(device.device, imgui_pool, nullptr);
    });
}

void Engine::init_triangle_pipeline() {

    VkShaderModule triangle_vert_shader;
    if(!vk_file::load_shader_module("../shaders/triangle.vert.spv", device.device, &triangle_vert_shader)) {
        fmt::print("Error loading triangle vert shader\n");
    } else {
        fmt::print("Loaded triangle vert shader successfully\n");
    }

    VkShaderModule triangle_frag_shader;
    if(!vk_file::load_shader_module("../shaders/triangle.frag.spv", device.device, &triangle_frag_shader)) {
        fmt::print("Error loading triangle frag shader\n");
    } else {
        fmt::print("Loaded triangle frag shader successfully\n");
    }

    // get empty pipeline layout
    VkPipelineLayoutCreateInfo layout_info = vk_init::get_pipeline_layout_create_info({}, {});
    VK_CHECK(vkCreatePipelineLayout(device.device, &layout_info, nullptr, &triangle_pipeline_layout));

    PipelineBuilder builder;
    builder.layout = triangle_pipeline_layout;
    builder.set_shaders(triangle_vert_shader, triangle_frag_shader);
    builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder.set_rasterizer_polygon_mode(VK_POLYGON_MODE_FILL);
    builder.set_rasterizer_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    builder.set_multisampling_none();
    builder.disable_blending();
    builder.enable_depth_test(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    builder.set_color_attachment_format(draw_image.format);
    builder.set_depth_format(depth_image.format);

    triangle_pipeline = builder.build_pipeline(device.device);

    vkDestroyShaderModule(device.device, triangle_vert_shader, nullptr);
    vkDestroyShaderModule(device.device, triangle_frag_shader, nullptr);

    engine_deletion_queue.push_function([&]() {
        vkDestroyPipelineLayout(device.device, triangle_pipeline_layout, nullptr);
        vkDestroyPipeline(device.device, triangle_pipeline, nullptr);
    });

}


void Engine::init_mesh_pipeline() {
    VkShaderModule mesh_vert_shader;
    if(!vk_file::load_shader_module("../shaders/colored_triangle_mesh.vert.spv", device.device, &mesh_vert_shader)) {
        fmt::print("Error loading colored triangle mesh vert shader\n");
    } else {
        fmt::print("Loaded colored triangle mesh vert shader successfully\n");
    }

    VkShaderModule triangle_frag_shader;
    if(!vk_file::load_shader_module("../shaders/triangle.frag.spv", device.device, &triangle_frag_shader)) {
        fmt::print("Error loading triangle frag shader\n");
    } else {
        fmt::print("Loaded triangle frag shader successfully\n");
    }

    VkPushConstantRange buffer_range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(GPUDrawPushConstants),
    };

    // fmt::print("Graphics Push constant size: {}\n", sizeof(GPUDrawPushConstants));

    std::vector<VkPushConstantRange> mesh_push_constant_ranges {
        buffer_range
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = vk_init::get_pipeline_layout_create_info({}, mesh_push_constant_ranges);
    VK_CHECK(vkCreatePipelineLayout(device.device, &pipeline_layout_info, nullptr, &mesh_pipeline_layout));

    PipelineBuilder builder;
    builder.layout = mesh_pipeline_layout;
    builder.set_shaders(mesh_vert_shader, triangle_frag_shader);
    builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder.set_rasterizer_polygon_mode(VK_POLYGON_MODE_FILL);
    builder.set_rasterizer_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    builder.set_multisampling_none();
    builder.enable_blending_additive();
    builder.disable_depth_test();

    builder.set_color_attachment_format(draw_image.format);
    builder.set_depth_format(VK_FORMAT_UNDEFINED);

    mesh_pipeline = builder.build_pipeline(device.device);

    vkDestroyShaderModule(device.device, mesh_vert_shader, nullptr);
    vkDestroyShaderModule(device.device, triangle_frag_shader, nullptr);

    engine_deletion_queue.push_function([&]() {
        vkDestroyPipelineLayout(device.device, mesh_pipeline_layout, nullptr);
        vkDestroyPipeline(device.device, mesh_pipeline, nullptr);
    });
}

GPUMeshBuffers Engine::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices, const std::string& mesh_name) {

    /*fmt::print("Vertex pos offset: {}\n", offsetof(Vertex, pos));
    fmt::print("Vertex normal offset: {}\n", offsetof(Vertex, normal));
    fmt::print("Vertex tangent offset: {}\n", offsetof(Vertex, tangent));
    fmt::print("Vertex color offset: {}\n", offsetof(Vertex, color));
    fmt::print("Vertex texCoord offset: {}\n", offsetof(Vertex, texCoord));
    fmt::print("Vertex texCoord1 offset: {}\n", offsetof(Vertex, texCoord1));
    fmt::print("Vertex material offset: {}\n", offsetof(Vertex, material));*/

    const size_t vertex_buffer_size = sizeof(Vertex) * vertices.size();
    const size_t index_buffer_size = sizeof(uint32_t) * indices.size();

    /*fmt::print("vertex buffer size: {}\n", vertex_buffer_size);
    fmt::print("index buffer size: {}\n", index_buffer_size);

    for(int i = 0; i < vertices.size(); i++) {
        fmt::print("Vertex {}'s position: ({}, {}, {})\n", i, vertices[i].pos.x, vertices[i].pos.y, vertices[i].pos.z);
    }*/

    GPUMeshBuffers new_surface;
    new_surface.vertex_buffer.init(allocator, vertex_buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                   VMA_MEMORY_USAGE_GPU_ONLY);
    new_surface.vertex_buffer.set_name(device.device, (mesh_name + std::string(" vertex buffer")).c_str());

    VkBufferDeviceAddressInfo device_address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = new_surface.vertex_buffer.buffer
    };

    new_surface.vertex_buffer_address = vkGetBufferDeviceAddress(device.device, &device_address_info);

    new_surface.index_buffer.init(allocator, index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VMA_MEMORY_USAGE_GPU_ONLY);
    new_surface.index_buffer.set_name(device.device, (mesh_name + std::string(" index buffer")).c_str());

    Buffer staging_buffer;
    staging_buffer.init(allocator, vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    staging_buffer.set_name(device.device, (mesh_name + std::string(" Index/Vertex Staging Buffer")).c_str());

    void* data = staging_buffer.allocation->GetMappedData();

    // copy vertex buffer data
    memcpy(data, vertices.data(), vertex_buffer_size);
    memcpy((char*)data + vertex_buffer_size, indices.data(), index_buffer_size);

    immediate_submit_command_buffer.submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertex_copy {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertex_buffer_size
        };

        vkCmdCopyBuffer(cmd, staging_buffer.buffer, new_surface.vertex_buffer.buffer, 1, &vertex_copy);

        VkBufferCopy index_copy {
                .srcOffset = vertex_buffer_size,
                .dstOffset = 0,
                .size = index_buffer_size
        };

        vkCmdCopyBuffer(cmd, staging_buffer.buffer, new_surface.index_buffer.buffer, 1, &index_copy);
    });

    staging_buffer.destroy_buffer();

    return new_surface;
}

void Engine::load_models() {
    std::shared_ptr<Model> model = gltf_loader.load("../models/basicmesh.glb", true);

    for(int mesh_index = 0; mesh_index < model->meshes.size(); mesh_index++) {
        Mesh& mesh = model->meshes[mesh_index];

        DrawData draw_data;
        draw_data.gpu_mesh_buffers = upload_mesh(mesh.indices, mesh.vertices, "monkey");

        // find all associated indirect draw datas
        for(auto& f : model->indirectDrawDataSet) {
            if(f.meshId == mesh_index) {
                draw_data.submesh_datas.push_back(f);
            }
        }

        draw_datas.push_back(std::make_shared<DrawData>(std::move(draw_data)));
    }

    /*// check the vertices make sense
    for(Vertex v : model->meshes[2].vertices) {
        fmt::print("Vertex: ({}, {}, {})\n", v.pos.x, v.pos.y, v.pos.z);
    }

    for(uint32_t i : model->meshes[2].indices) {
        fmt::print("Index: ({})\n", i);
    }*/

    engine_deletion_queue.push_function([&]() {
        for(auto& draw_data : draw_datas) {
            draw_data->gpu_mesh_buffers.vertex_buffer.destroy_buffer();
            draw_data->gpu_mesh_buffers.index_buffer.destroy_buffer();
        }
    });
}

void Engine::init_draw_and_depth_images() {

    VkExtent3D draw_image_extent = {
            .width = window.width,
            .height = window.height,
            .depth = 1
    };

    // DRAW image
    // Create an image to draw into each frame, it will eventually be copied into the swapchain
    draw_image.extent = draw_image_extent;
    draw_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    VkImageUsageFlags draw_image_usage_flags = {};
    draw_image_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    draw_image_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    draw_image_usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    draw_image_usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo draw_image_create_info = vk_init::get_image_create_info(draw_image.format, draw_image_usage_flags, draw_image.extent);

    VmaAllocationCreateInfo draw_image_alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vmaCreateImage(allocator, &draw_image_create_info, &draw_image_alloc_info, &draw_image.image, &draw_image.allocation, nullptr));

    VkImageViewCreateInfo draw_image_view_create_info = vk_init::get_image_view_create_info(draw_image.format, draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device.device, &draw_image_view_create_info, nullptr, &draw_image.view));

    // DEPTH image
    depth_image.extent = draw_image_extent;
    depth_image.format = VK_FORMAT_D32_SFLOAT;

    VkImageUsageFlags depth_image_usage_flags = {};
    depth_image_usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo depth_image_alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkImageCreateInfo depth_image_create_info = vk_init::get_image_create_info(depth_image.format, depth_image_usage_flags, depth_image.extent);
    VK_CHECK(vmaCreateImage(allocator, &depth_image_create_info, &depth_image_alloc_info, &depth_image.image, &depth_image.allocation, nullptr));

    VkImageViewCreateInfo depth_image_view_create_info = vk_init::get_image_view_create_info(depth_image.format, depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(device.device, &depth_image_view_create_info, nullptr, &depth_image.view));


    engine_deletion_queue.push_function([=, this]() {
        vkDestroyImageView(device.device, draw_image.view, nullptr);
        vmaDestroyImage(allocator, draw_image.image, draw_image.allocation);

        vkDestroyImageView(device.device, depth_image.view, nullptr);
        vmaDestroyImage(allocator, depth_image.image, depth_image.allocation);
    });

}

void Engine::resize_swapchain() {
    vkDeviceWaitIdle(device.device);
    swapchain.cleanup();
    swapchain.init(device.device,
                   vulkan_context.surface,
                   physical_device.surface_capabilities,
                   physical_device.surface_formats,
                   physical_device.present_modes,
                   window);
    swapchain_resize_requested = false;
}
