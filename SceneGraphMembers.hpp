//
// Created by darby on 1/11/2025.
//

#pragma once

#include "Common.hpp"
#include "GraphicsTypes.hpp"
#include "AllocatedImage.hpp"
#include "Buffer.hpp"
#include "DescriptorAllocatorGrowable.hpp"

class IRenderable {
    virtual void draw(const glm::mat4& top_matrix, DrawContext& ctx) = 0;
};

struct Node : public IRenderable {

    std::weak_ptr<Node> parent; // weak ptr to avoid circular dep.
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 local_transform;
    glm::mat4 world_transform;

    void refresh_transform(const glm::mat4& parent_matrix);
    virtual void draw(const glm::mat4& top_matrix, DrawContext& ctx);

};

struct MeshNode : public Node {
    // std::shared_ptr<MeshDrawData> mesh;
    std::shared_ptr<GLTFMesh> mesh;

    virtual void draw(const glm::mat4& top_matrix, DrawContext& draw_context) override;
};


struct GLTFFile : public IRenderable {
//    std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes;
    std::vector<std::shared_ptr<GLTFMesh>> meshes;
    // std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::vector<std::shared_ptr<Node>> nodes;
    // std::unordered_map<std::string, AllocatedImage> images;
    std::vector<AllocatedImage> images;
    // std::unordered_map<std::string, std::shared_ptr<GLTFMaterialData>> materials;
    std::vector<std::shared_ptr<Material>> materials;

    std::vector<std::shared_ptr<Node>> top_nodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable material_descriptor_pool;

    Buffer material_data_buffer;

    virtual void draw(const glm::mat4& top_matrix, DrawContext& draw_context) override;
    void destroy(VkDevice device);


};

