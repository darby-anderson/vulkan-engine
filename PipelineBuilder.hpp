//
// Created by darby on 12/18/2024.
//

#pragma once

#include "Common.hpp"

class PipelineBuilder {

public:

    enum BlendOptions {
        NoBlend,
        AdditiveBlend,
        AlphaBlend
    };

    // shader module infos
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    // triangle topology configuration - tris, lines, points?
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    // how to raster the triangles between vertex and fragment stages
    VkPipelineRasterizationStateCreateInfo rasterizer;
    // how to blend and write to the color attachment
    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;
    // the format of the color attachment image
    std::vector<VkFormat> color_attachment_formats;
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
    void enable_blending_additive_for_single_attachment();
    void enable_blending_alphablend();
    void set_single_color_attachment_format(VkFormat format);
    void set_multiple_color_attachment_formats_and_blending_styles(std::vector<VkFormat> formats, std::vector<PipelineBuilder::BlendOptions> blend_options);
    void disable_color_attachment();
    void set_depth_format(VkFormat format);
    void disable_depth_test();
    void enable_depth_test(bool enable_depth_write, VkCompareOp op);

    void clear();

    VkPipeline build_pipeline(VkDevice device, const std::string& name = "");

private:

    static void set_blending_additive(VkPipelineColorBlendAttachmentState& color_blend_attachment);

    static void set_blending_alphablend(VkPipelineColorBlendAttachmentState& color_blend_attachment);

    static void set_disable_blending(VkPipelineColorBlendAttachmentState& color_blend_attachment);

};

