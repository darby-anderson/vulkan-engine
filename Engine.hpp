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
#include "DescriptorWriter.hpp"
#include "GLTFHDRMaterial.hpp"
#include "SceneGraphMembers.hpp"
#include "SimpleCamera.hpp"
#include "EngineStats.hpp"
#include "ForwardRenderer.hpp"
#include "DeferredRenderer.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "Input.hpp"
#include "ShadowPipeline.hpp"


struct FrameData {
    VkCommandPool command_pool;
    VkCommandBuffer main_command_buffer;
    VkCommandBuffer main_command_buffer_2;

    // swapchain semaphore is used tell the command buffer executor we have a swapchain image ready to render into
    // render semaphore is used to tell the presentation engine the image is rendered
    VkSemaphore swapchain_semaphore, render_semaphore;
    // the render fence is used to inform the CPU the command buffer has finished processing
    VkFence render_fence;

    DeletionQueue deletion_queue;
    DescriptorAllocatorGrowable frame_descriptors;

    VkDescriptorSet light_data_descriptor_set;
};

constexpr uint32_t FRAME_OVERLAP = 2;

class Engine {
public:

    // initializes all components
    void init();

    // shuts down the engine
    void cleanup();

    void draw();

    // run the main loop
    void run();

    FrameData& get_current_frame();

private:
    void init_command_structures();
    void init_synch_objects();
    void init_descriptors();
    void init_pipelines();
    void init_draw_and_depth_images();
    void init_default_data();
    void init_imgui();
    void init_background_pipelines();
    void init_tone_mapping_pipeline();
    void init_renderers();

    void load_gltf_file(const std::string& file_path);

    void imgui_new_frame();
    void draw_background(VkCommandBuffer cmd);
    void draw_shadow_map(VkCommandBuffer cmd);
    void draw_geometry(VkCommandBuffer cmd);
    void draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view);
    void do_tone_mapping(VkCommandBuffer cmd);

    void resize_swapchain();

    void update_scene();

    VulkanContext vulkan_context;
    Window window;
    Input input_module;
    SimpleCamera camera;
    PhysicalDevice physical_device;
    Device device;
    Swapchain swapchain;
    VmaAllocator allocator;

    AllocatedImage draw_image;
    // AllocatedImage depth_image;
    VkExtent2D draw_extent;

    ForwardRenderer forward_renderer;
    DeferredRenderer deferred_renderer;

    // Shadows
    std::shared_ptr<ShadowPipeline> shadow_pipeline;
    AllocatedImage shadow_map_image;
    VkDescriptorSetLayout shadow_map_descriptor_set_layout;

    DescriptorAllocatorGrowable engine_descriptor_allocator;
    // VkDescriptorSet draw_image_descriptors;
    // VkDescriptorSetLayout draw_image_descriptor_layout;

    ComputePipeline gradient_pipeline;
    ComputePipeline sky_pipeline;

    ComputePipeline tone_mapping_pipeline;
    ToneMappingComputePushConstants tone_mapping_data {
        .exposure = 1.0f,
        .tone_mapping_strategy = 0
    };

    FrameData frames[FRAME_OVERLAP];
    uint32_t frame_number;

    GPUSceneData scene_data;
    // ^ uploaded to descriptor set associated with V
    VkDescriptorSetLayout gpu_scene_descriptor_set_layout;

    LightSourceData light_source_data;
    VkDescriptorSetLayout light_source_descriptor_set_layout;

    GLTFHDRMaterial hdr_material;

    // Default Data
    GPUMeshBuffers rectangle;

    AllocatedImage default_white_image;
    AllocatedImage default_grey_image;
    AllocatedImage default_black_image;
    AllocatedImage error_checkerboard_image;

    VkSampler default_nearest_neighbor_sampler;
    VkSampler default_linear_sampler;

    MaterialInstance default_material;
    // End Default Data

    ImmediateSubmitCommandBuffer immediate_submit_command_buffer;

    GLTFLoader gltf_loader;

    DeletionQueue engine_deletion_queue;

    // Interactive state
    std::vector<ComputeEffect> compute_effects;
    int current_compute_effect{1};
    float render_scale = 1.0f;
    float ambient_occlusion_strength = 1.0f;
    float sunlight_angle = 180.0f;
    float sunlight_light_distance = 10.0f;
    float shadow_bias_scalar = 0.0001f;
    bool use_perspective_light_projection = false;
    int shadow_softening_kernel_size = 3;

    float hdr_exposure = 1.0f;
    int tone_mapping_strategy_index = 0;
    const char* tone_mapping_strategies[3] = {
            "None",
            "Simple RGB Reinhard",
            "Hable-Filmic/Uncharted 2"
    };


    // Model Data
    // DescriptorAllocatorGrowable model_descriptor_allocator;

    EngineStats stats;

    DrawContext main_draw_context;
    std::unordered_map<std::string, std::shared_ptr<GLTFFile>> loaded_scenes;

    bool swapchain_resize_requested = false;

};
