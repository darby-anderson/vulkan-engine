//
// Created by darby on 12/19/2024.
//

#pragma once

#include "Common.hpp"
#include "Buffer.hpp"

/*struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};*/

struct Material;

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 view_proj;
    glm::vec4 ambient_color;
    glm::vec4 sunlight_direction;
    glm::vec4 sunlight_color;
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

struct GLTFMaterialData {
    int baseColorTextureId = -1;
    int baseColorSamplerId = -1;
    int metallicRoughnessTextureId = -1;
    int metallicRoughnessSamplerId = -1;
    int normalTextureTextureId = -1;
    int normalTextureSamplerId = -1;
    int emissiveTextureId = -1;
    int emissiveSamplerId = -1;
    float metallicFactor = 1.0;
    float roughnessFactor = 1.0;
    glm::vec2 padding; // needed to align the struct

    glm::vec4 baseColor;
};

struct ModelImageData {
    ModelImageData(const std::vector<uint8_t>& imageData, int width, int height, int channels) :
            ownedImageData_(std::make_unique<std::vector<uint8_t>>(imageData)),
            width(width),
            height(height),
            channels(channels)
    {
        data = ownedImageData_->data();
    }

    void* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;

private:
    std::unique_ptr<std::vector<uint8_t>> ownedImageData_;
};

struct Mesh {
    using Indices = uint32_t;
    std::vector<Vertex> vertices = {}; // replace with buffers?
    std::vector<Indices> indices = {}; // ^^^^^^^
    glm::vec3 minAABB = glm::vec3(999999, 999999, 999999);
    glm::vec3 maxAABB = glm::vec3(-999999, -999999, -999999);
    glm::vec3 extents;
    glm::vec3 center;
    int32_t material = -1;

    std::string name;
};

struct IndirectDrawDataAndMeshData {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t vertexOffset;
    uint32_t firstInstance;

    uint32_t meshId;

    int materialId;
    std::optional<std::shared_ptr<Material>> material; // where we place the loaded
};

struct Model {
    std::vector<Mesh> meshes;
    std::vector<GLTFMaterialData> materials;
    std::vector<std::unique_ptr<ModelImageData>> textures;

    std::vector<IndirectDrawDataAndMeshData> indirectDrawDataSet;

    GPUMeshBuffers mesh_buffers;

    uint32_t totalVertexSize = 0;
    uint32_t totalIndexSize = 0;
    uint32_t indexCount = 0;

    std::string name;
};

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

struct MeshDrawData {
    std::string name;

    std::vector<IndirectDrawDataAndMeshData> submesh_datas;
    GPUMeshBuffers gpu_mesh_buffers;
};


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
