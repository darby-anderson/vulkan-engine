//
// Created by darby on 12/11/2024.
//

#include "ComputePipeline.hpp"
#include "VulkanFileLoaderUtility.hpp"
#include "VulkanInitUtility.hpp"

void ComputePipeline::init(VkDevice device, const char* spv_path,
                           const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts,
                           const std::vector<VkPushConstantRange>& push_constant_ranges,
                           DeletionQueue& main_deletion_queue) {

    ASSERT(pipeline == VK_NULL_HANDLE, "ComputePipeline being created a second time");

    // PIPELINE LAYOUT
    VkPipelineLayoutCreateInfo compute_layout = vk_init::get_pipeline_layout_create_info(descriptor_set_layouts, push_constant_ranges);

    VK_CHECK(vkCreatePipelineLayout(device, &compute_layout, nullptr, &layout));

    // PIPELINE SHADERS
    VkShaderModule compute_draw_shader;
    ASSERT(vk_file::load_shader_module(spv_path, device, &compute_draw_shader), "Error loading compute draw shader");

    VkPipelineShaderStageCreateInfo shader_stage_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = compute_draw_shader,
            .pName = "main"
    };

    VkComputePipelineCreateInfo pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .stage = shader_stage_create_info,
            .layout = layout,
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));


    // destroy shader mod
    vkDestroyShaderModule(device, compute_draw_shader, nullptr);
    main_deletion_queue.push_function([&]() {
        vkDestroyPipelineLayout(device, layout, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
    });

}
