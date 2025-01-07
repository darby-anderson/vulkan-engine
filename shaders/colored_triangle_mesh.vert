#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;

/* original
struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};*/

struct Vertex {
    vec3 position; // 12 bytes
    uint buf;
    vec3 normal; // 12 bytes
    uint buf1;
    vec4 tangent; // 16 bytes
    vec3 color; // 12 bytes
    uint buf2;
    vec2 texCoord; // 8 bytes
    vec2 texCoord1; // 8 bytes
    uint material; // 4 bytes
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform constants {
    mat4 render_matrix;
    VertexBuffer vertex_buffer;
} PushConstants;

void main() {
    Vertex v = PushConstants.vertex_buffer.vertices[gl_VertexIndex];

    gl_Position = PushConstants.render_matrix * vec4(v.position, 1.0f);
    outColor = v.color.xyz;
    outUV.x = v.texCoord.x;
    outUV.y = v.texCoord.y;
}

