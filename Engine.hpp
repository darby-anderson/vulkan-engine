//
// Created by darby on 11/20/2024.
//
#pragma once

#include "Common.hpp"
#include "Window.hpp"
#include "VulkanContext.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Swapchain.hpp"
#include "AllocatedImage.hpp"
#include "DescriptorAllocatorGrowable.hpp"
#include "ComputePipeline.hpp"
#include "DeletionQueue.hpp"
#include "ComputeEffect.hpp"
#include "GraphicsTypes.hpp"
#include "GLTFLoader.hpp"
#include "ImmediateSubmitCommandBuffer.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"


struct FrameData {
    VkCommandPool command_pool;
    VkCommandBuffer main_command_buffer;

    // swapchain semaphore is used tell the command buffer executor we have a swapchain image ready to render into
    // render semaphore is used to tell the presentation engine the image is rendered
    VkSemaphore swapchain_semaphore, render_semaphore;
    // the render fence is used to inform the CPU the command buffer has finished processing
    VkFence render_fence;

    DeletionQueue deletion_queue;
    DescriptorAllocatorGrowable frame_descriptors;
};

constexpr uint32_t FRAME_OVERLAP = 2;

class Engine {
public:

    VulkanContext vulkan_context;
    Window window;
    PhysicalDevice physical_device;
    Device device;
    Swapchain swapchain;
    VmaAllocator allocator;

    AllocatedImage draw_image;
    AllocatedImage depth_image;
    VkExtent2D draw_extent; // used to
    float render_scale = 1.0f;

    DescriptorAllocatorGrowable global_descriptor_allocator;
    VkDescriptorSet draw_image_descriptors;
    VkDescriptorSetLayout draw_image_descriptor_layout;

    ComputePipeline gradient_pipeline;
    ComputePipeline sky_pipeline;

    VkPipeline triangle_pipeline;
    VkPipelineLayout triangle_pipeline_layout;

    VkPipeline mesh_pipeline;
    VkPipelineLayout mesh_pipeline_layout;

    FrameData frames[FRAME_OVERLAP];
    uint32_t frame_number;

    GPUSceneData scene_data;
    VkDescriptorSetLayout gpu_scene_descriptor_set_layout;

    ImmediateSubmitCommandBuffer immediate_submit_command_buffer;

    GLTFLoader gltf_loader;

    DeletionQueue engine_deletion_queue;

    // Interactive state
    std::vector<ComputeEffect> compute_effects;
    int current_compute_effect{0};

    // initializes all components
    void init();

    // shuts down the engine
    void cleanup();

    void draw();

    // run the main loop
    void run();

    FrameData& get_current_frame();

    GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices, const std::string& mesh_name = "");

private:
    void init_command_structures();
    void init_synch_objects();
    void init_descriptors();
    void init_pipelines();
    void init_draw_and_depth_images();
    void init_default_data();
    void init_imgui();
    void init_background_pipelines();
    void init_triangle_pipeline();
    void init_mesh_pipeline();
    void load_models();

    void imgui_new_frame();
    void draw_background(VkCommandBuffer cmd);
    void draw_geometry(VkCommandBuffer cmd);
    void draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view);

    void resize_swapchain();

    // data for each set of draw calls
    std::vector<std::shared_ptr<DrawData>> draw_datas;

    bool swapchain_resize_requested = false;

};
