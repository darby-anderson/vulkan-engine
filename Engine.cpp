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
#include "VulkanGeneralUtility.hpp"
#include "VulkanFileLoaderUtility.hpp"
#include "VulkanDebugUtility.hpp"
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

    input_module.init(&window);
    camera.init(&input_module);

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

    init_default_data();

    init_imgui();

}

/*
 * Init the target images for our pipelines, the color and depth attachment images.
 */
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
    vk_debug::name_resource<VkImage>(device.device, VK_OBJECT_TYPE_IMAGE, draw_image.image, "Draw Image");

    VkImageViewCreateInfo draw_image_view_create_info = vk_init::get_image_view_create_info(draw_image.format, draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device.device, &draw_image_view_create_info, nullptr, &draw_image.view));
    vk_debug::name_resource<VkImageView>(device.device, VK_OBJECT_TYPE_IMAGE_VIEW, draw_image.view, "Draw Image View");

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
    vk_debug::name_resource<VkImage>(device.device, VK_OBJECT_TYPE_IMAGE, depth_image.image, "Depth Image");

    VkImageViewCreateInfo depth_image_view_create_info = vk_init::get_image_view_create_info(depth_image.format, depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(device.device, &depth_image_view_create_info, nullptr, &depth_image.view));
    vk_debug::name_resource<VkImageView>(device.device, VK_OBJECT_TYPE_IMAGE_VIEW, depth_image.view, "Depth Image View");

    // SHADOW MAP IMAGE
    shadow_map_image.extent = VkExtent3D {
        .width = 4096,
        .height = 4096,
        .depth = 1,
    };
    shadow_map_image.format = VK_FORMAT_D32_SFLOAT;

    VkImageUsageFlags shadow_map_image_usage_flags = {};
    shadow_map_image_usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    shadow_map_image_usage_flags |= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VmaAllocationCreateInfo  shadow_map_alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkImageCreateInfo shadow_map_image_create_info = vk_init::get_image_create_info(shadow_map_image.format, shadow_map_image_usage_flags, shadow_map_image.extent);
    VK_CHECK(vmaCreateImage(allocator, &shadow_map_image_create_info, &shadow_map_alloc_info, &shadow_map_image.image, &shadow_map_image.allocation, nullptr));
    vk_debug::name_resource<VkImage>(device.device, VK_OBJECT_TYPE_IMAGE, shadow_map_image.image, "Shadow Map Image");

    VkImageViewCreateInfo shadow_map_image_view_create_info = vk_init::get_image_view_create_info(shadow_map_image.format, shadow_map_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(device.device, &shadow_map_image_view_create_info, nullptr, &shadow_map_image.view));
    vk_debug::name_resource<VkImageView>(device.device, VK_OBJECT_TYPE_IMAGE_VIEW, shadow_map_image.view, "Shadow Map Image View");

    engine_deletion_queue.push_function([=, this]() {
        vkDestroyImageView(device.device, draw_image.view, nullptr);
        vmaDestroyImage(allocator, draw_image.image, draw_image.allocation);

        vkDestroyImageView(device.device, depth_image.view, nullptr);
        vmaDestroyImage(allocator, depth_image.image, depth_image.allocation);

        vkDestroyImageView(device.device, shadow_map_image.view, nullptr);
        vmaDestroyImage(allocator, shadow_map_image.image, shadow_map_image.allocation);
    });

}

/*
 * Inits a command pool and command buffer for each for the 3 frames
 *  AND
 * Inits the class which handles immediate command buffer submissions
 */
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

/*
 * Create the global descriptor allocator, descriptor layouts, and allocate
 * space for the descriptor sets.
 * Then updates descriptor sets to push changes.
 */
void Engine::init_descriptors() {
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
            // dedicated 100% of the pool to storage image descriptor types
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
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

    // LIGHT SOURCE DESCRIPTOR SET LAYOUT
    DescriptorLayoutBuilder light_source_layout_builder;
    light_source_layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    light_source_descriptor_set_layout = light_source_layout_builder.build(device.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    // SHADOW MAP DESCRIPTOR SET LAYOUT
    DescriptorLayoutBuilder shadow_map_layout_builder;
    shadow_map_layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    shadow_map_descriptor_set_layout = shadow_map_layout_builder.build(device.device, VK_SHADER_STAGE_FRAGMENT_BIT);

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

    // SHADOW PIPELINE
    shadow_pipeline.init(device.device, shadow_map_image, light_source_descriptor_set_layout);
    engine_deletion_queue.push_function([&](){
        shadow_pipeline.destroy_resources(device.device);
    });

    // SECOND GRAPHICS PIPELINE -> METALLIC ROUGHNESS PIPELINE
    metal_rough_material.build_pipelines(device.device, light_source_descriptor_set_layout, gpu_scene_descriptor_set_layout, shadow_map_descriptor_set_layout, draw_image, depth_image);
    engine_deletion_queue.push_function([&](){
        metal_rough_material.clear_resources(device.device);
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


void Engine::init_default_data() {

    // Rectangle Data
    std::array<Vertex,4> rect_vertices;

    rect_vertices[0].pos = {0.5,-0.5, 0};
    rect_vertices[1].pos = {0.5,0.5, 0};
    rect_vertices[2].pos = {-0.5,-0.5, 0};
    rect_vertices[3].pos = {-0.5,0.5, 0};

    rect_vertices[0].color = {0,0, 0,1};
    rect_vertices[1].color = { 0.5,0.5,0.5 ,1};
    rect_vertices[2].color = { 1,0, 0,1 };
    rect_vertices[3].color = { 0,1, 0,1 };

    std::array<uint32_t,6> rect_indices;

    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;

    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    rectangle = vk_util::upload_mesh(rect_indices, rect_vertices, allocator, device.device, immediate_submit_command_buffer, "default rect");

    //delete the rectangle data on engine shutdown
    engine_deletion_queue.push_function([&](){
        rectangle.index_buffer.destroy_buffer();
        rectangle.vertex_buffer.destroy_buffer();
    });


    // Image data
    VkExtent3D default_image_extent = {
            .width = 1,
            .height = 1,
            .depth = 1
    };

    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    default_white_image.init_with_data(immediate_submit_command_buffer, device.device, allocator, (void*)&white, default_image_extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    default_grey_image.init_with_data(immediate_submit_command_buffer, device.device, allocator, (void*)&grey, default_image_extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1)); // alpha as 0?
    default_black_image.init_with_data(immediate_submit_command_buffer, device.device, allocator, (void*)&black, default_image_extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> checkerboard_pixels;
    for(uint32_t x = 0; x < 16; x++) {
        for(uint32_t y = 0; y < 16; y++) {
            if((x % 2) ^ (y % 2)) { // true when x and y have opposing parities (odd/even)
                checkerboard_pixels[y * 16 + x] = magenta;
            } else {
                checkerboard_pixels[y * 16 + x] = black;
            }
        }
    }

    VkExtent3D checkerboard_image_extent = {
            .width = 16,
            .height = 16,
            .depth = 1
    };
    error_checkerboard_image.init_with_data(immediate_submit_command_buffer, device.device, allocator, (void*)checkerboard_pixels.data(), checkerboard_image_extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    engine_deletion_queue.push_function([&]() {
        default_white_image.destroy(device.device);
        default_grey_image.destroy(device.device);
        default_black_image.destroy(device.device);
        error_checkerboard_image.destroy(device.device);
    });

    // Sampler data

    VkSamplerCreateInfo sampler_create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO
    };

    // nearest neighbor sampler
    sampler_create_info.magFilter = VK_FILTER_NEAREST;
    sampler_create_info.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(device.device, &sampler_create_info, nullptr, &default_nearest_neighbor_sampler);

    // linear sampler
    sampler_create_info.magFilter = VK_FILTER_LINEAR;
    sampler_create_info.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(device.device, &sampler_create_info, nullptr, &default_linear_sampler);

    engine_deletion_queue.push_function([&]() {
        vkDestroySampler(device.device, default_nearest_neighbor_sampler, nullptr);
        vkDestroySampler(device.device, default_linear_sampler, nullptr);
    });

    // CREATE DEFAULT MATERIAL
    GLTFMetallic_Roughness::MaterialResources material_resources;
    material_resources.color_image = default_white_image;
    material_resources.color_sampler = default_linear_sampler;
    material_resources.metal_rough_image = default_white_image;
    material_resources.metal_rough_sampler = default_linear_sampler;
    material_resources.normal_image = default_white_image;
    material_resources.normal_sampler = default_linear_sampler;
    material_resources.ambient_occlusion_image = default_white_image;
    material_resources.ambient_occlusion_sampler = default_linear_sampler;


    Buffer material_constants;
    material_constants.init(allocator, sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // Write the material constants buffer
    GLTFMetallic_Roughness::MaterialConstants* material_uniform_data = (GLTFMetallic_Roughness::MaterialConstants*)material_constants.allocation->GetMappedData();
    material_uniform_data->color_factors = glm::vec4(1, 1, 1, 1);
    material_uniform_data->metal_rough_factors = glm::vec4(1, 0, 0.5, 0);
    material_uniform_data->includes_certain_textures = glm::bvec4(true, true, true, false);
    material_uniform_data->normal_scale = 1.0f;
    material_uniform_data->ambient_occlusion_strength = 1.0f;

    engine_deletion_queue.push_function([=, this](){
        material_constants.destroy_buffer();
    });

    material_resources.data_buffer = material_constants.buffer;
    material_resources.data_buffer_offset = 0;

    default_material = metal_rough_material.write_material(device.device, MaterialPassType::MainColor, material_resources, global_descriptor_allocator);

    load_gltf_file("../models/ABeautifulGame/ABeautifulGame.gltf");

}

void Engine::load_gltf_file(const std::string& file_path) {
    // std::shared_ptr<Model> model = gltf_loader.load(file_path, override_color_with_normal);
    std::shared_ptr<GLTFFile> gltf_file = gltf_loader.load_file(device.device,
                                                                allocator,
                                                                metal_rough_material,
                                                                immediate_submit_command_buffer,
                                                                default_white_image,
                                                                default_nearest_neighbor_sampler,
                                                                file_path,
                                                                true);

    engine_deletion_queue.push_function([&]() {
       gltf_file->destroy(device.device);
    });

    std::string file_name = vk_file::extract_file_name_from_path(file_path.c_str());
    loaded_scenes[file_name] = gltf_file;
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



void Engine::run() {

    while(!window.should_exit()) {
        auto frame_start = std::chrono::system_clock::now();

        window.request_os_messages();

        if(!window.has_focus()) {
            // fmt::print("Sleeping\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        input_module.start_new_frame();

        imgui_new_frame();
        draw();

        if(swapchain_resize_requested) {
            fmt::print("resizing swapchain");
            resize_swapchain();
        }

        auto frame_end = std::chrono::system_clock::now();
        auto time_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start);
        stats.frame_time = time_elapsed.count() / 1000;

        if(stats.frame_time > stats.longest_frame_time) {
            stats.longest_frame_time = stats.frame_time;
            stats.longest_frame_number = frame_number;
        }

        frame_number++;
    }

}

void Engine::imgui_new_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if(ImGui::Begin("User Controls")) {

        if(ImGui::CollapsingHeader("Lighting Controls")) {
            ImGui::Text("Light Data");
            ImGui::SliderFloat("Sunlight Direction", &sunlight_angle, 0.0f, 360.0f);
            ImGui::Checkbox("Use Perspective Light Projection", &use_perspective_light_projection);
            ImGui::SliderFloat("(Perspective) Light Distance: ", &sunlight_light_distance, 1.0f, 20.0f);

            ImGui::Text("Shadow Data");
            ImGui::SliderFloat("Ambient Occlusion Strength", &ambient_occlusion_strength, 0.0f, 5.0f);
            ImGui::InputFloat("Shadow Bias Scalar", &shadow_bias_scalar, 0.0f, 0.0f, "%.6f");
            ImGui::Text("Shadow Softening Kernel Size: %d", shadow_softening_kernel_size);
            if (ImGui::RadioButton("1", shadow_softening_kernel_size == 1))
            {
                shadow_softening_kernel_size = 1;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("3", shadow_softening_kernel_size == 3))
            {
                shadow_softening_kernel_size = 3;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("5", shadow_softening_kernel_size == 5))
            {
                shadow_softening_kernel_size = 5;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("7", shadow_softening_kernel_size == 7))
            {
                shadow_softening_kernel_size = 7;
            }


        }

        if(ImGui::CollapsingHeader("Background Controls")) {
            ComputeEffect& selected = compute_effects[current_compute_effect];
            ImGui::Text("Selected effect: %s", selected.name);
            ImGui::SliderInt("Effect Index", &current_compute_effect, 0, compute_effects.size() - 1);
            ImGui::InputFloat4("data1", (float*) &selected.data.data1);
            ImGui::InputFloat4("data2", (float*) &selected.data.data2);
        }

    }
    ImGui::End();

    if(ImGui::Begin("Stats")) {
        ImGui::Text("Frame Time: %f ms", stats.frame_time);
        ImGui::Text("Longest Frame Time: %f ms. On frame # %d", stats.longest_frame_time, stats.longest_frame_number);
        ImGui::Text("Draw Time: %f ms", stats.mesh_draw_time);
        ImGui::Text("Update Scene Function Time: %f ms", stats.scene_update_time);
        ImGui::Text("Triangle Count: %i", stats.triangle_count);
        ImGui::Text("Draw Count %i", stats.draw_call_count);
        glm::vec3 cam_pos = camera.get_position();
        ImGui::Text("Camera position: (%f, %f, %f)", cam_pos.x, cam_pos.y, cam_pos.z);
    }
    ImGui::End();

    ImGui::Render();
}

void Engine::draw() {

    update_scene();

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

    draw_extent.width = std::min(swapchain.extent.width, draw_image.extent.width); //  * render_scale; // accounts for if the swapchain is larger than the draw image
    draw_extent.height = std::min(swapchain.extent.height, draw_image.extent.height); //  * render_scale;

    VkCommandBuffer cmd = get_current_frame().main_command_buffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo begin_info = vk_init::get_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    // draw shadow map TODO
    // set up shadow map image to hold depth information
    vk_image::transition_image_layout(cmd, shadow_map_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_shadow_map(cmd);

    // set up shadow map image to be read from
    vk_image::transition_image_layout_specify_aspect(cmd, shadow_map_image.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    // transition from undefined (image either created or in unknown state), to general (for the clear operation)
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

void Engine::update_scene() {
    auto update_scene_start = std::chrono::system_clock::now();

    camera.update();

    main_draw_context.opaque_surfaces.clear();

    loaded_scenes["ABeautifulGame.gltf"]->draw(glm::scale(glm::vec3(1.0f)), main_draw_context);

    glm::mat4 view = camera.get_view_matrix();
    glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)draw_image.extent.width / (float)draw_image.extent.height, 0.0001f, 10000.0f);
    projection[1][1] *= -1; // flip y direction

    scene_data.view = view;
    scene_data.proj = projection;
    scene_data.view_proj = projection * view;

    scene_data.ambient_color = glm::vec4(0.05f);

    ComputeEffect& effect = compute_effects[current_compute_effect];
    if(strcmp("gradient effect", effect.name) == 0) {
        glm::vec4 additional_color = glm::mix(effect.data.data1, effect.data.data2, 0.25f);
        scene_data.sunlight_color = glm::mix(glm::vec4(1.0f), additional_color, 0.5f);
    } else {
        scene_data.sunlight_color = glm::vec4(1.0f);
    }

    // set x and z based off of sunlight angle user input
    float r = 3.0f;
    float x = glm::cos(glm::radians(sunlight_angle));
    float z = glm::sin(glm::radians(sunlight_angle));
    scene_data.sunlight_direction = glm::vec4(x, 1, z, 1);

    scene_data.ambient_occlusion_strength = ambient_occlusion_strength;
    scene_data.shadow_bias_scalar = shadow_bias_scalar;
    scene_data.shadow_softening_kernel_size = shadow_softening_kernel_size;

    glm::vec3 sunlight_position_for_shadow = sunlight_light_distance * glm::normalize(glm::vec3(scene_data.sunlight_direction));

    // world space to light-projection-space
    float near_plane = 1.0f;
    float far_plane = sunlight_light_distance * 1.5f;
    float ortho_size = 0.5f;

    glm::mat4 light_projection;
    if(use_perspective_light_projection) {
        light_projection = glm::perspective(glm::radians(70.0f), 1.0f, near_plane, far_plane);
    } else {
        light_projection = glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, near_plane, far_plane);
    }
    light_projection[1][1] *= -1;

    glm::mat4 light_view_matrix = glm::lookAt(sunlight_position_for_shadow,
                                           glm::vec3(0.0f, 0.0f, 0.0f),
                                           glm::vec3(0.0f, 1.0f, 0.0f));

    light_source_data.light_view_matrix = light_view_matrix;
    light_source_data.light_projection_matrix = light_projection;

    auto update_scene_end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds >(update_scene_end - update_scene_start);
    stats.scene_update_time = elapsed.count() / 1000.f;
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


void Engine::draw_shadow_map(VkCommandBuffer cmd) {

    VkRenderingAttachmentInfo depth_attachment_info = vk_init::get_depth_attachment_info(shadow_map_image.view);
    VkExtent2D render_extent = {
            .width = shadow_map_image.extent.width,
            .height = shadow_map_image.extent.height
    };
    VkRenderingInfo render_info = vk_init::get_rendering_info(render_extent, {}, &depth_attachment_info);

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

    DescriptorAllocatorGrowable& frame_descriptor_allocator = get_current_frame().frame_descriptors;
    DeletionQueue& frame_deletion_queue = get_current_frame().deletion_queue;

    get_current_frame().light_data_descriptor_set = shadow_pipeline.create_frame_light_data_descriptor_set(
            device.device,
            light_source_data,
            allocator,
            frame_descriptor_allocator,
            frame_deletion_queue);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pipeline.pipeline_layout, 0, 1, &get_current_frame().light_data_descriptor_set, 0, nullptr);

    for(const RenderObject& draw : main_draw_context.opaque_surfaces) {
      // Tell the GPU which material-specific set of variables in memory we want to currently use
        vkCmdBindIndexBuffer(cmd, draw.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        GPUDrawPushConstants push_constants = {
                .world_matrix = draw.transform,
                .vertex_buffer_address = draw.vertex_buffer_address
        };
        vkCmdPushConstants(cmd, shadow_pipeline.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

        vkCmdDrawIndexed(cmd, draw.index_count, 1, draw.first_index, 0, 0);
    }

    vkCmdEndRendering(cmd);
}


void Engine::draw_geometry(VkCommandBuffer cmd) {

    stats.draw_call_count = 0;
    stats.triangle_count = 0;
    auto draw_geometry_start = std::chrono::system_clock::now();

    VkRenderingAttachmentInfo color_attachment = vk_init::get_color_attachment_info(draw_image.view, nullptr);
    std::vector<VkRenderingAttachmentInfo> color_attachment_infos = {
            color_attachment
    };
    VkRenderingAttachmentInfo depth_attachment_info = vk_init::get_depth_attachment_info(depth_image.view);

    VkRenderingInfo render_info = vk_init::get_rendering_info(swapchain.extent, color_attachment_infos, &depth_attachment_info);

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

    VkDescriptorSet shadow_map_descriptor_set = shadow_pipeline.create_frame_shadow_map_descriptor_set(device.device,
                                                                                                       shadow_map_image,
                                                                                                       default_linear_sampler,
                                                                                                       get_current_frame().frame_descriptors,
                                                                                                       shadow_map_descriptor_set_layout);

    // Create the GPU scene data buffer for this frame
    // This handles the data-race which may occur if we updated a uniform buffer being read-from by inflight shader executions
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

    for(const RenderObject& draw : main_draw_context.opaque_surfaces) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 0, 1, &global_descriptor_set, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 1, 1, &get_current_frame().light_data_descriptor_set, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 2, 1, &shadow_map_descriptor_set, 0, nullptr);

        // Tell the GPU which material-specific set of variables in memory we want to currently use
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 3, 1, &draw.material->material_set, 0, nullptr);

        vkCmdBindIndexBuffer(cmd, draw.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        GPUDrawPushConstants push_constants = {
                .world_matrix = draw.transform,
                .vertex_buffer_address = draw.vertex_buffer_address
        };
        vkCmdPushConstants(cmd, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

        vkCmdDrawIndexed(cmd, draw.index_count, 1, draw.first_index, 0, 0);

        stats.draw_call_count++;
        stats.triangle_count += draw.index_count / 3;
    }

    vkCmdEndRendering(cmd);

    auto draw_geometry_end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds >(draw_geometry_end - draw_geometry_start);
    stats.mesh_draw_time = elapsed.count() / 1000.f;
}

void Engine::draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view) {

    std::vector<VkRenderingAttachmentInfo> color_attachment_infos = {
            vk_init::get_color_attachment_info(target_image_view, nullptr)
    };
    VkRenderingInfo rendering_info = vk_init::get_rendering_info(draw_extent, color_attachment_infos, nullptr);

    vkCmdBeginRendering(cmd, &rendering_info);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}


FrameData& Engine::get_current_frame() {
    return frames[frame_number % FRAME_OVERLAP];
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
