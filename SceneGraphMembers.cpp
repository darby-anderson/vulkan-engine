//
// Created by darby on 1/11/2025.
//

#include "SceneGraphMembers.hpp"

void Node::refresh_transform(const glm::mat4& parent_matrix) {
    world_transform = parent_matrix * local_transform;
    for(auto c : children) {
        c->refresh_transform(world_transform);
    }
}

void Node::draw(const glm::mat4& top_matrix, DrawContext& ctx) {
    for(auto& c : children) {
        c->draw(top_matrix, ctx);
    }
}


void MeshNode::draw(const glm::mat4& top_matrix, DrawContext& draw_context) {

    glm::mat4 node_matrix = top_matrix * world_transform;

    for(auto& s : mesh->draw_datas) {
        RenderObject def = {};
        def.index_count = s.indexCount;
        def.first_index = s.firstIndex;
        def.index_buffer = mesh->mesh_buffers.index_buffer.buffer;
        def.material = &s.material.value()->data;

        def.transform = node_matrix;
        def.vertex_buffer_address = mesh->mesh_buffers.vertex_buffer_address;

        draw_context.opaque_surfaces.push_back(def);
    }

    Node::draw(top_matrix, draw_context);
}

void GLTFFile::draw(const glm::mat4& top_matrix, DrawContext& draw_context) {
    for(std::shared_ptr<Node> n : top_nodes) {
        n->draw(top_matrix, draw_context);
    }
}

void GLTFFile::destroy(VkDevice device) {

    // destroy all textures
    for(auto& i : images) {
        i.destroy(device);
    }

    // destroy mesh vertex/index buffers
    for(auto& m : meshes) {
        m->mesh_buffers.vertex_buffer.destroy_buffer();
        m->mesh_buffers.index_buffer.destroy_buffer();
    }

    material_data_buffer.destroy_buffer(); // handles all material constants
    material_descriptor_pool.destroy_descriptor_pools(device); // handles all material descriptors
}
