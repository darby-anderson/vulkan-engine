#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "brdf_input_structures_2.glsl"

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_frag_world_pos;
layout(location = 4) in vec4 in_tangent;
layout(location = 5) in vec4 in_light_space_pos;

layout(location = 0) out vec4 out_frag_color;

// According to GLTF impelmentation: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation-general

float heaviside(float val) {
    return step(0.0f, val);
}

// term 1 is dot(n, l) or dot(n, v)
// term 2 is dot(h, l) or dot(h, v)
float smith(float term1, float term2, float a2) {
    float smith_num = 2 * abs(term1) * heaviside(term2);
    float smith_denom = abs(term1) + sqrt(a2 + (1 - a2) * pow(term1, 2));
    return smith_num / smith_denom;
}


vec3 specular_brdf(float a, vec3 n, vec3 l, vec3 v, vec3 h) {
    // re-used functions
    float n_dot_h = dot(n, h);
    float n_dot_l = dot(n, l);
    float n_dot_v = dot(n, v);

    // trowbridge-reitz/ggx microfacet distribution (D)
    float a2 = pow(a, 2.0);
    float d_num = a2 * heaviside(n_dot_h);
    float d_denom = M_PI * pow(pow(n_dot_h, 2.0) * (a2 - 1) + 1, 2.0);
    float d = d_num / d_denom;

    // smith joint masking-shadowing function (G)
    float g = smith(n_dot_l, dot(h, l), a2) * smith(n_dot_v, dot(h, v), a2);

    // visibility function
    float visibility = g / (4 * abs(n_dot_l) * abs(n_dot_v));

    return vec3(visibility * d);
}

vec3 diffuse_brdf(vec3 base_color) {
    return (1 / M_PI) * base_color;
}

// Schlick's Approximation
vec3 fresnel_mix(float index_of_reflection, vec3 diffuse_base_color, vec3 specular_layer, float v_dot_h) {
    float f0 = pow((1 - index_of_reflection) / (1 + index_of_reflection), 2.0);
    float f = f0 + (1 - f0) * pow(1 - abs(v_dot_h), 5.0);
    return mix(diffuse_base_color, specular_layer, f);
}

vec3 conductor_fresnel(vec3 specular_bsdf, vec3 base_color, float v_dot_h) {
    return specular_bsdf * (base_color + (1 - base_color) * pow(1 - abs(v_dot_h), 5.0));
}

    // description of calculating the tangent and bitangent provided here: http://www.thetenthplanet.de/archives/1180
// N is the vertex normal in world space, p is inverse of view vec in world space
mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv) {
    // edge vectors of pixel triangle
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x; // tangent following the x-dir, adjusted by perspective
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y; // bi-tangent following the y-dir

    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

void main() {

    float ambient_occlusion = 1.0f + (material_data.ambient_occlusion_strength * scene_data.ambient_occlusion_scalar) * (texture(ambient_occlusion_tex, in_uv).x - 1.0f);

    // Base Color, corrected into linear space for sRGB encoded images (GLTF standard)
    vec3 base_color = pow((texture(color_tex, in_uv) * material_data.color_factors).xyz, vec3(2.2f));

    // vertex normal
    vec3 vertex_normal_ws = in_normal;

    // world-space view vec, shade_location to camera
    mat4 invView = inverse(scene_data.view);
    vec3 cam_pos_ws = vec3(invView[3]);
    vec3 v_ws = normalize(cam_pos_ws - in_frag_world_pos);

    // N (tangent-space surface normal)
    vec3 read_normal = texture(normal_tex, in_uv).xyz;
    vec3 mapped_normal = read_normal * 2.0 - 1.0;// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#additional-textures
    vec3 scaled_normal = mapped_normal * material_data.normal_tex_scalar;
    vec3 n = normalize(scaled_normal);

    // transpose is equivalent to the inverse here: https://learnopengl.com/Advanced-Lighting/Normal-Mapping
    mat3 TBN = transpose(cotangent_frame(in_normal, -v_ws, in_uv)); // use the transpose to go from world-space->tangent-space

    n = normalize(cotangent_frame(in_normal, -v_ws, in_uv) * n);

    // V (world-space view vec, shade location to camera)
    vec3 v =  normalize(v_ws);

    // L (world-space light vec, shade location to light)
    vec3 l = normalize( normalize(scene_data.sunlight_direction).xyz);

    // H (world-space half vec)
    vec3 h = normalize(v + l);

    vec3 metal_rough_val = texture(metal_rough_tex, in_uv).xyz;
    float roughness = metal_rough_val.g * material_data.metal_rough_factors[1];
    float metalness = metal_rough_val.b * material_data.metal_rough_factors[0];
    float a = pow(roughness, 2.0);

    // trowbridge-reitz/ggx microfacet distribution (D)
    float a2 = pow(a, 2.0);
    float d_num = a2 * heaviside(dot(n, h));
    float d_denom = M_PI * pow(pow(dot(n, h), 2.0) * (a2 - 1) + 1, 2.0);
    float d = d_num / max(d_denom, 0.0001f);

    // smith joint masking-shadowing function (G)
    float g = smith(dot(n, l), dot(h, l), a2) * smith(dot(n, v), dot(h, v), a2);

    vec3 black = vec3(0.0f);
    vec3 white = vec3(1.0f);

    vec3 c_diff = mix(base_color.rgb, black, metalness);
    vec3 f0 = mix(vec3(0.04), base_color.rgb, metalness);

    vec3 f = f0 + (white - f0) * pow(1.0f - abs(dot(v, h)), 5.0f);

    vec3 f_diffuse = (white - f) * (1 / M_PI) * c_diff;
    vec3 f_specular = f * d * g / (4.0f * abs(dot(v, n) * abs(dot(l, n))));

    vec3 material = (f_diffuse + f_specular) * scene_data.sunlight_color.xyz;

    // ambient color
    vec3 ambient_2 = vec3(0.03) * base_color * vec3(ambient_occlusion);

    // shadow value
    vec3 proj_light_space_coords = in_light_space_pos.xyz / in_light_space_pos.w;
    proj_light_space_coords = vec3(proj_light_space_coords.xy * 0.5 + 0.5, proj_light_space_coords.z);
    // ^^^ xy are in -1,1 space, z is in 0,1 space

    float first_z_in_light_space = texture(shadow_map_tex, proj_light_space_coords.xy).r;
    float current_depth = proj_light_space_coords.z;
    // bias for shadow acne
    mat4 invLightView = inverse(light_source_data.light_view_matrix);
    vec3 light_dir = normalize(vec3(invLightView[3]));
    float bias = max(scene_data.shadow_bias_scalar * (1.0 - dot(in_normal, light_dir)), scene_data.shadow_bias_scalar / 10.0); //  0.0; // 0.0005;
    // ^^ bias determined by similarity between the normal and the light dir as more sheer angles will cause worse
    // sampling patterns/shadow acne

    float shadow_average = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow_map_tex, 0);
    int kernel_edge = scene_data.shadow_softening_kernel_size / 2;
    for(int x = -kernel_edge; x <=kernel_edge; x++) {
        for(int y = -kernel_edge; y <= kernel_edge; y++) {
            float first_z_in_light_space = texture(shadow_map_tex, proj_light_space_coords.xy + vec2(x, y) * texelSize).r;
            shadow_average += current_depth - bias > first_z_in_light_space ? 1.0 : 0.0;
        }
    }

    shadow_average /= pow(scene_data.shadow_softening_kernel_size, 2.0);

    vec3 color_2 = (1.0 - shadow_average) * material + ambient_2;

    out_frag_color = vec4(color_2, 1.0f);
}

