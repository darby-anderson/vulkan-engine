#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "common_pipeline_structures.glsl"
// My changes to the original file were not being respected until I changed the name VV
#include "brdf_input_structures_2.glsl"

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec3 out_color;
layout(location = 2) out vec2 out_UV;
layout(location = 3) out vec3 out_world_pos;
layout(location = 4) out vec4 out_tangent;
layout(location = 5) out vec4 out_light_space_pos;

void main() {
    Vertex v = PushConstants.vertex_buffer.vertices[gl_VertexIndex];

    vec4 position = vec4(v.position, 1.0f);
    vec4 world_space_pos = PushConstants.model_matrix * position;
    out_world_pos = world_space_pos.xyz;
    vec4 view_space_pos = scene_data.view * world_space_pos;

    gl_Position = scene_data.proj * view_space_pos;

    out_normal = (PushConstants.model_matrix * vec4(v.normal, 0.f)).xyz; // world space
    out_color = v.color.xyz * material_data.color_factors.xyz;
    out_UV.x = v.texCoord.x;
    out_UV.y = v.texCoord.y;
    out_tangent = v.tangent;

    mat4 light_matrix = light_source_data.light_projection_matrix * light_source_data.light_view_matrix;
    out_light_space_pos = light_matrix * vec4(out_world_pos, 1.0f);
}
