//
// Created by darby on 12/19/2024.
//

#pragma once

#include "Common.hpp"
#include "Buffer.hpp"

struct Material;

struct GPUSceneData {
    glm::mat4 view; // 16 * 4 - 64
    glm::mat4 proj; // 128
    glm::mat4 view_proj; // 196
    glm::vec4 ambient_color;
    glm::vec4 sunlight_direction;
    glm::vec4 sunlight_color;
    float ambient_occlusion_strength;
    float shadow_bias_scalar;
    int shadow_softening_kernel_size;
};

struct LightSourceData {
    glm::mat4 light_view_matrix;
    glm::mat4 light_projection_matrix;
};

struct GPUMeshBuffers {
    Buffer index_buffer;
    Buffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
};

struct GPUDrawPushConstants {
    glm::mat4 world_matrix;
    VkDeviceAddress vertex_buffer_address;
};

struct ToneMappingComputePushConstants {
    float exposure;
    uint32_t tone_mapping_strategy; // 0 - No Tone Mapping, 1 - Reinhard Tone Mapping
};

struct alignas(16) Vertex {
    glm::vec3 pos = glm::vec3();
    uint32_t  buf = 0;
    glm::vec3 normal = glm::vec3();
    uint32_t buf1 = 0;
    glm::vec4 tangent = glm::vec4();
    glm::vec4 color = glm::vec4();
    glm::vec2 texCoord = glm::vec2();
    glm::vec2 texCoord1 = glm::vec2();
    uint32_t material;

    void applyTransform(const glm::mat4& m) {
        auto newp = m * glm::vec4(pos, 1.0);
        pos = glm::vec3(newp.x, newp.y, newp.z);
        glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(m));
        normal = normalMatrix * normal;
        tangent = glm::inverseTranspose(m) * tangent;
    }
};

// GLTF Loader data structures VVVVVV

struct SurfaceDrawData {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t vertexOffset;
    uint32_t firstInstance;

    uint32_t meshId;

    int materialId;
    std::optional<std::shared_ptr<Material>> material;
};

struct GLTFMesh {
    std::vector<SurfaceDrawData> draw_datas;
    GPUMeshBuffers mesh_buffers;
};

// ^^^^ GLTF Loader data structures

enum class MaterialPassType : uint8_t {
    MainColor,
    Transparent,
    Other
};


struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet material_set;
    MaterialPassType pass_type;
};

struct Material {
    MaterialInstance data;
};

struct RenderObject {
    uint32_t index_count;
    uint32_t first_index;
    VkBuffer index_buffer;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertex_buffer_address;
};

struct DrawContext {
    std::vector<RenderObject> opaque_surfaces;
};
