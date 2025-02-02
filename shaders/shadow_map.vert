#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "common_pipeline_structures.glsl"

layout(set = 0, binding = 0) uniform LightSourceData {
    mat4 light_view_matrix;
    mat4 light_projection_matrix;
} light_source_data;

void main() {
    Vertex v = PushConstants.vertex_buffer.vertices[gl_VertexIndex];
    vec4 position = vec4(v.position, 1.0f);

    vec4 vert_position_ws = PushConstants.model_matrix * position;
    vec4 vert_position_ls = light_source_data.light_view_matrix * vert_position_ws;
    vec4 vert_position_projective_ls = light_source_data.light_projection_matrix * vert_position_ls;
    // vert_position_projective_ls[2] *= -1;

    gl_Position = vert_position_projective_ls;
}
