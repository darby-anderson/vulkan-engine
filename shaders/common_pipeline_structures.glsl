
struct Vertex {
    vec3 position; // 12 bytes
    uint buf;
    vec3 normal; // 12 bytes
    uint buf1;
    vec4 tangent; // 16 bytes
    vec4 color; // 16 bytes
    vec2 texCoord; // 8 bytes
    vec2 texCoord1; // 8 bytes
    uint material; // 4 bytes
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform constants {
    mat4 model_matrix;
    VertexBuffer vertex_buffer;
} PushConstants;


