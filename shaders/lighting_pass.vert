#version 450

#extension GL_EXT_buffer_reference : require

struct DeferredLightingTriangleVertex {
    vec3 position; // 12 bytes
    uint buf;
    vec2 texCoord; // 12 bytes
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    DeferredLightingTriangleVertex vertices[];
};

layout(push_constant) uniform constants {
    VertexBuffer vertex_buffer;
} PushConstants;

layout(location = 0) out vec2 out_UV;

void main() {
    DeferredLightingTriangleVertex v = PushConstants.vertex_buffer.vertices[gl_VertexIndex];
    vec4 position = vec4(v.position, 1.0f);

    gl_Position = position;

    out_UV = v.texCoord;
}
