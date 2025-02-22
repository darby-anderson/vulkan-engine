#version 450

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_frag_world_pos;
layout(location = 4) in vec4 in_tangent;
// layout(location = 5) in vec4 in_light_space_pos;

layout(location = 0) out vec4 out_world_normal;
layout(location = 1) out vec4 out_albedo;

void main() {

    // NORMAL G-BUFFER
    // vertex normal
    vec3 vertex_normal_ws = in_normal;

    // world-space view vec, shade_location to camera
//    mat4 invView = inverse(scene_data.view);
//    vec3 cam_pos_ws = vec3(invView[3]);
//    vec3 v_ws = normalize(cam_pos_ws - in_frag_world_pos);

    // N (tangent-space surface normal)
//    vec3 read_normal = texture(normal_tex, in_uv).xyz;
//    vec3 mapped_normal = read_normal * 2.0 - 1.0;// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#additional-textures
//    vec3 scaled_normal = mapped_normal * material_data.normal_tex_scalar;
//    vec3 n = normalize(scaled_normal);

    // transpose is equivalent to the inverse here: https://learnopengl.com/Advanced-Lighting/Normal-Mapping
    // mat3 TBN = transpose(cotangent_frame(in_normal, -v_ws, in_uv)); // use the transpose to go from world-space->tangent-space

    // n = normalize(cotangent_frame(in_normal, -v_ws, in_uv) * n);
    out_world_normal = vec4(in_normal, 1.0);

    // ALBEDO G-BUFFER
    // float ambient_occlusion = 1.0f + (material_data.ambient_occlusion_strength * scene_data.ambient_occlusion_scalar) * (texture(ambient_occlusion_tex, in_uv).x - 1.0f);

    // Base Color, corrected into linear space for sRGB encoded images (GLTF standard)
//    vec3 base_color = pow((texture(color_tex, in_uv) * material_data.color_factors).xyz, vec3(2.2f));

    vec3 base_color = pow(in_color, vec3(2.2f));

    // ambient color
    // vec3 ambient_2 = vec3(0.03) * base_color * vec3(ambient_occlusion);

    out_albedo = vec4(base_color, 1.0f);

    // METAL/ROUGHNESS G-BUFFERS, etc.
//
//    vec3 metal_rough_val = texture(metal_rough_tex, in_uv).xyz;
//    float roughness = metal_rough_val.g * material_data.metal_rough_factors[1];
//    float metalness = metal_rough_val.b * material_data.metal_rough_factors[0];
//    float a = pow(roughness, 2.0);


}


