#version 450


// UNIFORMS
// --------- SCENE DATA
layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec4 ambient_color;
    vec4 sunlight_direction; // user-provided
    vec4 sunlight_color;
    float ambient_occlusion_scalar; // user-provided
    float shadow_bias_scalar; // user-provided
    int shadow_softening_kernel_size; // user-provided
} scene_data;

// ---------- LIGHT DATA
layout(set = 1, binding = 0) uniform LightSourceData {
    mat4 light_view_matrix;
    mat4 light_projection_matrix;
} light_source_data;

// ---------- G-BUFFER DATA
layout(set = 2, binding = 0) uniform sampler2D depth_g_buffer;
layout(set = 2, binding = 1) uniform sampler2D world_normal_g_buffer;
layout(set = 2, binding = 2) uniform sampler2D albedo_g_buffer;

// VARYING DATA
layout(location = 0) in vec2 in_UV;

layout(location = 0) out vec4 out_frag_color;

void main() {

    out_frag_color = vec4(texture(albedo_g_buffer, in_UV).xyz, 1.0f);
}
