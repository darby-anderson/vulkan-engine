#define M_PI 3.1415926535897932384626433832795

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

layout(set = 1, binding = 0) uniform LightSourceData {
    mat4 light_view_matrix;
    mat4 light_projection_matrix;
} light_source_data;

layout(set = 2, binding = 0) uniform sampler2D shadow_map_tex;

layout(set = 3, binding = 0) uniform GLTFMaterialData {
    vec4 color_factors;
    vec4 metal_rough_factors; // [0] - metallness factor, [1] - roughness factor
    float normal_tex_scalar;
    float ambient_occlusion_strength;
    // [0] - includes color texture
    // [1] - includes metal_rough texture
    // [2] - includes normal texture
    // [3] - includes ambient occlusion texture
    bvec4 includes_certain_textures;
} material_data;

layout(set = 3, binding = 1) uniform sampler2D color_tex;
layout(set = 3, binding = 2) uniform sampler2D metal_rough_tex;
layout(set = 3, binding = 3) uniform sampler2D normal_tex;
layout(set = 3, binding = 4) uniform sampler2D ambient_occlusion_tex;



//struct Vertex {
//    vec3 position; // 12 bytes
//    uint buf;
//    vec3 normal; // 12 bytes
//    uint buf1;
//    vec4 tangent; // 16 bytes
//    vec4 color; // 16 bytes
//    vec2 texCoord; // 8 bytes
//    vec2 texCoord1; // 8 bytes
//    uint material; // 4 bytes
//};
//
//layout(buffer_reference, std430) readonly buffer VertexBuffer {
//    Vertex vertices[];
//};
//
//layout(push_constant) uniform constants {
//    mat4 model_matrix;
//    VertexBuffer vertex_buffer;
//} PushConstants;

