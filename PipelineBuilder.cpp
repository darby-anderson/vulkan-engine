//
// Created by darby on 12/18/2024.
//

#include "PipelineBuilder.hpp"

#include "VulkanInitUtility.hpp"
#include "VulkanDebugUtility.hpp"

void PipelineBuilder::clear() {

    input_assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };

    rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
    };

    color_blend_attachment = {};

    multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
    };

    layout = {};

    depth_stencil = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };

    render_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO
    };

    shader_stages.clear();

}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, const std::string& name) {

    // NON-CONFIGURABLE STRUCTS

    VkPipelineViewportStateCreateInfo viewport_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .viewportCount = 1,
            .scissorCount = 1,
    };

    // No transparency blend
    VkPipelineColorBlendStateCreateInfo blend_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment,
    };

    // No need for this
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
    };

    VkDynamicState state[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = &state[0]
    };

    // End of non-configurable structures

    VkGraphicsPipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &render_info, // necessary for dynamic rendering
            .stageCount = static_cast<uint32_t>(shader_stages.size()),
            .pStages = shader_stages.data(),
            .pVertexInputState = &vertex_input_info,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_info,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depth_stencil,
            .pColorBlendState = &blend_info,
            .pDynamicState = &dynamic_state_info,
            .layout = layout,
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &pipeline_info, nullptr, &pipeline));

    if(!name.empty()) {
        vk_debug::name_resource<VkPipeline>(device, VK_OBJECT_TYPE_PIPELINE, pipeline, name.c_str());
    }

    return pipeline;
}

void PipelineBuilder::set_shaders(VkShaderModule vertex_shader, VkShaderModule fragment_shader) {
    shader_stages.clear();

    shader_stages.push_back(vk_init::get_pipeline_shader_stage_info(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader));
    shader_stages.push_back(vk_init::get_pipeline_shader_stage_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader));
}

void PipelineBuilder::set_input_topology(VkPrimitiveTopology topology) {
    input_assembly.topology = topology;
    input_assembly.primitiveRestartEnable = false;
}

void PipelineBuilder::set_rasterizer_polygon_mode(VkPolygonMode polygon_mode) {
    rasterizer.polygonMode = polygon_mode;
    rasterizer.lineWidth = 1.f;
}

void PipelineBuilder::set_rasterizer_cull_mode(VkCullModeFlags cull_mode, VkFrontFace front_face) {
    rasterizer.cullMode = cull_mode;
    rasterizer.frontFace = front_face;
}

void PipelineBuilder::set_multisampling_none() {
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disable_blending() {
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::set_color_attachment_format(VkFormat format) {
    color_attachment_format = format;

    // need to also inform the render info struct
    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachmentFormats = &color_attachment_format;
}

void PipelineBuilder::disable_color_attachment() {
    render_info.colorAttachmentCount = 0;
}


void PipelineBuilder::set_depth_format(VkFormat format) {
    render_info.depthAttachmentFormat = format;
}

void PipelineBuilder::disable_depth_test() {
    depth_stencil.depthTestEnable = VK_FALSE;
    depth_stencil.depthWriteEnable = VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {};
    depth_stencil.back = {};
    depth_stencil.minDepthBounds = 0.f;
    depth_stencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::enable_depth_test(bool enable_depth_write, VkCompareOp op) {
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = enable_depth_write;
    depth_stencil.depthCompareOp = op;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {};
    depth_stencil.back = {};
    depth_stencil.minDepthBounds = 0.f;
    depth_stencil.maxDepthBounds = 1.f;
}

// blending works like: "outColor = srcColor * srcColorBlendFactor <op> dstColor * dstColorBlendFactor;"
void PipelineBuilder::enable_blending_additive() {
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enable_blending_alphablend() {
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}










