//
// Created by darby on 12/18/2024.
//

#pragma once

#include "Common.hpp"

class PipelineBuilder {

public:

    // shader module infos
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    // triangle topology configuration - tris, lines, points?
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    // how to raster the triangles between vertex and fragment stages
    VkPipelineRasterizationStateCreateInfo rasterizer;
    // how to blend and write to the color attachment
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    // the format of the color attachment image
    VkFormat color_attachment_format;
    // used to set up anti-aliasing
    VkPipelineMultisampleStateCreateInfo multisampling;
    // describes the resources used by the pipeline - descriptor set layouts, push constant ranges
    VkPipelineLayout layout;
    // depth-testing info
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    // hold the attachment formats the pipeline will use - necessary for dynamic rendering
    VkPipelineRenderingCreateInfo render_info;

    PipelineBuilder() { clear(); }

    void set_shaders(VkShaderModule vertex_shader, VkShaderModule fragment_shader);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_rasterizer_polygon_mode(VkPolygonMode polygon_mode);
    void set_rasterizer_cull_mode(VkCullModeFlags cull_mode, VkFrontFace front_face);
    void set_multisampling_none();
    void disable_blending();
    void enable_blending_additive();
    void enable_blending_alphablend();
    void set_color_attachment_format(VkFormat format);
    void disable_color_attachment();
    void set_depth_format(VkFormat format);
    void disable_depth_test();
    void enable_depth_test(bool enable_depth_write, VkCompareOp op);

    void clear();

    VkPipeline build_pipeline(VkDevice device, const std::string& name = "");

};

