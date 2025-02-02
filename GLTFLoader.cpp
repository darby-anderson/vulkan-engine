//
// Created by darby on 7/17/2024.
//
#include <iostream>

#include "volk.h"

#include "GLTFLoader.hpp"
#include "Buffer.hpp"
#include "VulkanGeneralUtility.hpp"


#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION // need to define this after including GLTFLoader.hpp since it also includes tiny_gltf
#include "tiny_gltf.h"


// TINY GLTF CODE vvv
//    int minFilter =
//            -1;  // optional. -1 = no filter defined. ["NEAREST", "LINEAR",
//    // "NEAREST_MIPMAP_NEAREST", "LINEAR_MIPMAP_NEAREST",
//    // "NEAREST_MIPMAP_LINEAR", "LINEAR_MIPMAP_LINEAR"]
//    int magFilter =
//            -1;  // optional. -1 = no filter defined. ["NEAREST", "LINEAR"]
VkFilter get_filter(int filter_id) {
    switch(filter_id) {
        case 0:
            return VK_FILTER_NEAREST;
        case 1:
            return VK_FILTER_LINEAR;
    }

    return VK_FILTER_LINEAR;
}

VkSamplerMipmapMode get_mipmap_mode(int filter_id) {
    switch(filter_id) {
        case 2:
        case 3:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case 4:
        case 5:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

std::shared_ptr<GLTFFile> GLTFLoader::load_file(VkDevice device, VmaAllocator allocator,
                                                GLTFMetallic_Roughness& material_creator,
                                                ImmediateSubmitCommandBuffer& immediate_submit_command_buffer,
                                                AllocatedImage texture_load_error_image, VkSampler texture_load_error_sampler,
                                                const std::string& filePath, bool override_color_with_normal) {

    std::shared_ptr<GLTFFile> out_gltf = std::make_shared<GLTFFile>();
    GLTFFile& outputModel = *out_gltf.get();

    auto gltfPath = std::filesystem::current_path();
    gltfPath /= filePath;

    tinygltf::TinyGLTF loader;
    std::shared_ptr<tinygltf::Model> tinyModel = std::make_shared<tinygltf::Model>();
    std::string err;
    std::string warn;

    bool loadedCorrectly;

    if (filePath.ends_with(".glb")) {
        loadedCorrectly = loader.LoadBinaryFromFile(tinyModel.get(), &err, &warn, gltfPath.string());
    } else if (filePath.ends_with(".gltf")) {
        loadedCorrectly = loader.LoadASCIIFromFile(tinyModel.get(), &err, &warn, gltfPath.string());
    } else {
        fmt::print("Cannot load this type of model file\n");
        loadedCorrectly = false;
    }

    if (!warn.empty()) {
        fmt::print("Warn: {}\n", warn.c_str());
    }

    if (!err.empty()) {
        fmt::print("Err: {}\n", err.c_str());
    }

    if (!loadedCorrectly) {
        fmt::print("Failed to parse glTF\n");
        exit(-1);
    }


    // Allocate descriptor pool for materials
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };
    out_gltf->material_descriptor_pool.init_allocator(device, tinyModel->materials.size(), sizes);

    // Load samplers
    for(tinygltf::Sampler sampler : tinyModel->samplers) {
        VkSamplerCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .pNext = nullptr,
                .magFilter = get_filter(sampler.magFilter),
                .minFilter = get_filter(sampler.minFilter),
                .mipmapMode = get_mipmap_mode(sampler.minFilter),
                .minLod = 0,
                .maxLod = VK_LOD_CLAMP_NONE,
        };

        VkSampler new_sampler;
        vkCreateSampler(device, &info, nullptr, &new_sampler);

        out_gltf->samplers.push_back(new_sampler);
    }

    // Load images
//    out_gltf->images.reserve(tinyModel->textures.size());
//    for(int i = 0; i < tinyModel->textures.size(); i++) {
//        tinygltf::Texture& texture = tinyModel->textures[i];
//        tinygltf::Image& image = tinyModel->images[texture.source];
//
//        VkExtent3D extent = {
//                .width = static_cast<uint32_t>(image.width),
//                .height = static_cast<uint32_t>(image.height),
//                .depth = 1
//        };
//
//        out_gltf->images[i].init_with_data(immediate_submit_command_buffer, device, allocator, image.image.data(), extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
//    }

    out_gltf->images.resize(tinyModel->images.size());
    for(int i = 0; i < tinyModel->images.size(); i++) {
        tinygltf::Image& image = tinyModel->images[i];
        VkExtent3D extent = {
                .width = static_cast<uint32_t>(image.width),
                .height = static_cast<uint32_t>(image.height),
                .depth = 1
        };

        out_gltf->images[i].init_with_data(immediate_submit_command_buffer, device, allocator, image.image.data(), extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
    }


    // Load materials
    size_t material_buffer_size = sizeof(GLTFMetallic_Roughness::MaterialConstants) * tinyModel->materials.size();
    out_gltf->material_data_buffer.init(allocator, material_buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    int material_data_index = 0;
    auto scene_material_constants = (GLTFMetallic_Roughness::MaterialConstants*) out_gltf->material_data_buffer.info.pMappedData;

    out_gltf->materials.resize(tinyModel->materials.size());
    for(int i = 0; i < tinyModel->materials.size(); i++) {
        tinygltf::Material& tiny_mat_data = tinyModel->materials[i];

        fmt::print("material extensions: {}\n", tiny_mat_data.extensions_json_string);

        int base_color_tex_index = tiny_mat_data.pbrMetallicRoughness.baseColorTexture.index;
        int metal_rough_tex_index = tiny_mat_data.pbrMetallicRoughness.metallicRoughnessTexture.index;
        int normal_texture_index = tiny_mat_data.normalTexture.index;
        int ambient_occlusion_tex_index = tiny_mat_data.occlusionTexture.index;

        bool includes_color_texture = base_color_tex_index != -1;
        bool includes_metal_roughness_texture = metal_rough_tex_index != -1;
        bool includes_normal_texture = normal_texture_index != -1;
        bool includes_ambient_occlusion_texture = ambient_occlusion_tex_index != -1;

        // Fill material constants
        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.color_factors.x = tiny_mat_data.pbrMetallicRoughness.baseColorFactor[0];
        constants.color_factors.y = tiny_mat_data.pbrMetallicRoughness.baseColorFactor[1];
        constants.color_factors.z = tiny_mat_data.pbrMetallicRoughness.baseColorFactor[2];
        constants.color_factors.w = tiny_mat_data.pbrMetallicRoughness.baseColorFactor[3];

        constants.metal_rough_factors.x = tiny_mat_data.pbrMetallicRoughness.metallicFactor;
        constants.metal_rough_factors.y = tiny_mat_data.pbrMetallicRoughness.roughnessFactor;

        constants.includes_certain_textures[0] = includes_color_texture;
        constants.includes_certain_textures[1] = includes_metal_roughness_texture;
        constants.includes_certain_textures[2] = includes_normal_texture;
        constants.includes_certain_textures[3] = includes_ambient_occlusion_texture;

        constants.ambient_occlusion_strength = tiny_mat_data.occlusionTexture.strength;
        constants.normal_scale = tiny_mat_data.normalTexture.scale;

        // fmt::print("vec4 size: {}, bvec4 size: {}\n", sizeof(glm::vec4), sizeof(glm::bvec4));

        scene_material_constants[i] = constants;

        // Pass Type

        MaterialPassType pass_type = MaterialPassType::MainColor;
        if(tiny_mat_data.alphaMode == "TRANSPARENT") {
            pass_type = MaterialPassType::Transparent;
        }

        // Fill material resources

        GLTFMetallic_Roughness::MaterialResources resources;

        if(includes_color_texture) { // COLOR
            tinygltf::Texture& texture = tinyModel->textures[base_color_tex_index];
            resources.color_image = out_gltf->images[texture.source];
            resources.color_sampler = out_gltf->samplers[texture.sampler];
        } else {
            resources.color_image = texture_load_error_image;
            resources.color_sampler = texture_load_error_sampler;
        }

        if(includes_metal_roughness_texture) { // METAL ROUGHNESS
            tinygltf::Texture& texture = tinyModel->textures[metal_rough_tex_index];
            resources.metal_rough_image = out_gltf->images[texture.source];
            resources.metal_rough_sampler = out_gltf->samplers[texture.sampler];
        } else {
            resources.metal_rough_image = texture_load_error_image;
            resources.metal_rough_sampler = texture_load_error_sampler;
        }

        if(includes_normal_texture) { // NORMAL
            tinygltf::Texture& texture = tinyModel->textures[normal_texture_index];
            resources.normal_image = out_gltf->images[texture.source];
            resources.normal_sampler = out_gltf->samplers[texture.sampler];
        } else {
            resources.normal_image = texture_load_error_image;
            resources.normal_sampler = texture_load_error_sampler;
        }

        if(includes_ambient_occlusion_texture) { // AMBIENT_OCCLUSION
            tinygltf::Texture& texture = tinyModel->textures[ambient_occlusion_tex_index];
            resources.ambient_occlusion_image = out_gltf->images[texture.source];
            resources.ambient_occlusion_sampler = out_gltf->samplers[texture.sampler];
        } else {
            resources.ambient_occlusion_image = texture_load_error_image;
            resources.ambient_occlusion_sampler = texture_load_error_sampler;
        }

        resources.data_buffer = out_gltf->material_data_buffer.buffer;
        resources.data_buffer_offset = i * sizeof(GLTFMetallic_Roughness::MaterialConstants);

        std::shared_ptr<Material> our_material = std::make_shared<Material>();
        our_material->data = material_creator.write_material(device, pass_type, resources, out_gltf->material_descriptor_pool);

        out_gltf->materials[i] = our_material;
    }

    // Load meshes
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    out_gltf->meshes.reserve(tinyModel->meshes.size());
    for(int i = 0; i < tinyModel->meshes.size(); i++) {
        tinygltf::Mesh& tiny_mesh = tinyModel->meshes[i];
        std::shared_ptr<GLTFMesh> mesh = std::make_shared<GLTFMesh>();

        indices.clear();
        vertices.clear();

        for(int p = 0; p < tiny_mesh.primitives.size(); p++) {
            tinygltf::Primitive& primitive = tiny_mesh.primitives[p];

            SurfaceDrawData new_draw_data;
            new_draw_data.firstIndex = static_cast<uint32_t>(indices.size());

            tinygltf::Accessor indices_accessor = tinyModel->accessors[primitive.indices];
            new_draw_data.indexCount = static_cast<uint32_t>(indices_accessor.count);

            size_t initial_vertex = vertices.size();

            // indices
            {
                indices.reserve(indices.size() + indices_accessor.count);

                const tinygltf::BufferView& indicesBufferView = tinyModel->bufferViews[indices_accessor.bufferView];
                const tinygltf::Buffer& indicesBuffer = tinyModel->buffers[indicesBufferView.buffer];

                if(indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* convertedData = reinterpret_cast<const uint16_t *>(&indicesBuffer.data[indices_accessor.byteOffset + indicesBufferView.byteOffset]);
                    for(uint32_t index_index = 0; index_index < indices_accessor.count; index_index++) {
                        // get the index, and add the current number of vertices across all surfaces already handled in mesh
                        indices.push_back(static_cast<uint16_t>(convertedData[index_index] + initial_vertex));
                    }
                } else if(indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* convertedData = reinterpret_cast<const uint32_t *>(&indicesBuffer.data[indices_accessor.byteOffset + indicesBufferView.byteOffset]);
                    for(uint32_t index_index = 0; index_index < indices_accessor.count; index_index++) {
                        // get the index, and add the current number of vertices across all surfaces already handled in mesh
                        indices.push_back(static_cast<uint32_t>(convertedData[index_index] + initial_vertex));
                    }
                }
            }

            // vert positions
            {
                auto position_accessor_index = primitive.attributes.find("POSITION")->second;
                tinygltf::Accessor position_accessor = tinyModel->accessors[position_accessor_index];

                vertices.resize(vertices.size() + position_accessor.count);

                const tinygltf::BufferView& positionBufView = tinyModel->bufferViews[position_accessor.bufferView];
                const tinygltf::Buffer& positionBuf = tinyModel->buffers[positionBufView.buffer];

                const glm::vec3* position_data = reinterpret_cast<const glm::vec3*>(&positionBuf.data[position_accessor.byteOffset + positionBufView.byteOffset]);
                for(int v = 0; v < position_accessor.count; v++) {
                    Vertex vertex = {
                            .pos = position_data[v],
                            .normal = glm::vec3(1, 0, 0),
                            .tangent = glm::vec4(0.f),
                            .color = glm::vec4(1.f),
                            .texCoord = glm::vec2(0.f),
                    };

                    // can't use vertices.size() since we resized
                    vertices[initial_vertex + v] = vertex;
                }
            }

            // vert normals
            {
                auto normal_accessor_iterator = primitive.attributes.find("NORMAL");
                if(normal_accessor_iterator != primitive.attributes.end()) {
                    auto normal_accessor = tinyModel->accessors[normal_accessor_iterator->second];
                    const tinygltf::BufferView& normal_buf_view = tinyModel->bufferViews[normal_accessor.bufferView];
                    const tinygltf::Buffer& normal_buf = tinyModel->buffers[normal_buf_view.buffer];

                    const glm::vec3* normal_data = reinterpret_cast<const glm::vec3*>(&normal_buf.data[normal_accessor.byteOffset + normal_buf_view.byteOffset]);
                    for(int v = 0; v < normal_accessor.count; v++) {
                        vertices[initial_vertex + v].normal = normal_data[v];
                    }
                }
            }

            // vert uvs
            {
                auto texcoord_accessor_iterator = primitive.attributes.find("TEXCOORD_0");
                if(texcoord_accessor_iterator != primitive.attributes.end()) {
                    auto texcoord_accessor = tinyModel->accessors[texcoord_accessor_iterator->second];
                    const tinygltf::BufferView& texcoord_buf_view = tinyModel->bufferViews[texcoord_accessor.bufferView];
                    const tinygltf::Buffer& texcoord_buf = tinyModel->buffers[texcoord_buf_view.buffer];

                    const glm::vec2* texcoord_data = reinterpret_cast<const glm::vec2*>(&texcoord_buf.data[texcoord_accessor.byteOffset + texcoord_buf_view.byteOffset]);
                    for(int v = 0; v < texcoord_accessor.count; v++) {
                        vertices[initial_vertex + v].texCoord = texcoord_data[v];
                    }
                }
            }

            // vert colors
            {
                auto color_accessor_iterator = primitive.attributes.find("COLOR_0");
                if(color_accessor_iterator != primitive.attributes.end()) {
                    auto color_accessor = tinyModel->accessors[color_accessor_iterator->second];
                    const tinygltf::BufferView& color_buf_view = tinyModel->bufferViews[color_accessor.bufferView];
                    const tinygltf::Buffer& color_buf = tinyModel->buffers[color_buf_view.buffer];

                    const glm::vec4* color_data = reinterpret_cast<const glm::vec4*>(&color_buf.data[color_accessor.byteOffset + color_buf_view.byteOffset]);
                    for(int v = 0; v < color_accessor.count; v++) {
                        vertices[initial_vertex + v].color = color_data[v];
                    }
                }
            }

            // vert tangent
            {
                auto tangent_accessor_iterator = primitive.attributes.find("TANGENT");
                if(tangent_accessor_iterator != primitive.attributes.end()) {
                    auto tangent_accessor = tinyModel->accessors[tangent_accessor_iterator->second];
                    const tinygltf::BufferView& tangent_buf_view = tinyModel->bufferViews[tangent_accessor.bufferView];
                    const tinygltf::Buffer& tangent_buf = tinyModel->buffers[tangent_buf_view.buffer];

                    const glm::vec4* tangent_data = reinterpret_cast<const glm::vec4*>(&tangent_buf.data[tangent_accessor.byteOffset + tangent_buf_view.byteOffset]);
                    for(int v = 0; v < tangent_accessor.count; v++) {
                        vertices[initial_vertex + v].tangent = tangent_data[v];
                    }
                } else {
                    fmt::print("File does not contain tangent!\n");
                }
            }

            // set draw data's material
            new_draw_data.material = primitive.material == -1 ? out_gltf->materials[0] : out_gltf->materials[primitive.material];

            mesh->draw_datas.push_back(new_draw_data);
        }

        // upload mesh data to the GPU
        mesh->mesh_buffers = vk_util::upload_mesh(indices, vertices, allocator, device, immediate_submit_command_buffer, tiny_mesh.name);

        out_gltf->meshes.push_back(mesh);
    }

    for(int n = 0; n < tinyModel->nodes.size(); n++) {
        tinygltf::Node& tiny_node = tinyModel->nodes[n];
        std::shared_ptr<Node> new_node;

        if(tiny_node.mesh != -1) {
            new_node = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(new_node.get())->mesh = out_gltf->meshes[tiny_node.mesh];
        } else {
            new_node = std::make_shared<Node>();
        }

        out_gltf->nodes.push_back(new_node);

        glm::mat4 m(1.0);
        if(tinyModel->nodes[n].matrix.size() == 16) {
            // use matrix attribute
            for(int i = 0; i < 4; i++) {
                for(int j = 0; j < 4; j++) {
                    new_node->local_transform[i][j] = static_cast<float>(tinyModel->nodes[n].matrix[(i * 4) + j]);
                }
            }
        } else {
            // use Translate x Rotate x Scale order
            glm::mat4 mScale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
            if(!tinyModel->nodes[n].scale.empty()) {
                mScale = glm::scale(glm::mat4(1.0f), glm::vec3(tinyModel->nodes[n].scale[0], tinyModel->nodes[n].scale[1], tinyModel->nodes[n].scale[2]));
            }

            glm::mat4 mRot = glm::mat4_cast(glm::quat(1, 0, 0, 0));
            if(!tinyModel->nodes[n].rotation.empty()) {
                mRot = glm::mat4_cast(glm::quat(tinyModel->nodes[n].rotation[3], tinyModel->nodes[n].rotation[0], tinyModel->nodes[n].rotation[1], tinyModel->nodes[n].rotation[2]));
            }

            glm::mat4 mTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
            if(!tinyModel->nodes[n].translation.empty()) {
                mTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(tinyModel->nodes[n].translation[0], tinyModel->nodes[n].translation[1], tinyModel->nodes[n].translation[2]));
            }

            new_node->local_transform = mTranslate * mRot * mScale;
        }
    }

    // create hierarchy
    for(int n = 0; n < tinyModel->nodes.size(); n++) {
        tinygltf::Node& tiny_node = tinyModel->nodes[n];
        std::shared_ptr<Node> node = out_gltf->nodes[n];

        for(auto& c : tiny_node.children) {
            node->children.push_back(out_gltf->nodes[c]);
            out_gltf->nodes[c] = node;
        }
    }

    // find top nodes w/ no parents
    for(int n = 0; n < out_gltf->nodes.size(); n++) {
        std::shared_ptr<Node> node = out_gltf->nodes[n];

        if(node->parent.lock() == nullptr) {
            out_gltf->top_nodes.push_back(node);
            node->refresh_transform(glm::mat4(1.f));
        }
    }

    return out_gltf;
}

/*
 *

namespace {
    std::vector<unsigned char> imageData(std::shared_ptr<tinygltf::Model> tinyModel, uint32_t imageId) {
        tinygltf::Image& image = tinyModel->images[imageId];
        return image.image;
    }
}

std::shared_ptr<Model> GLTFLoader::load(const std::string& filePath, bool override_color_with_normal) {

    std::shared_ptr<Model> outModel = std::make_shared<Model>();
    Model& outputModel = *outModel.get();

    auto gltfPath = std::filesystem::current_path();
    gltfPath /= filePath;

    tinygltf::TinyGLTF loader;
    std::shared_ptr<tinygltf::Model> tinyModel = std::make_shared<tinygltf::Model>();
    std::string err;
    std::string warn;

    bool loadedCorrectly;

    if(filePath.ends_with(".glb")) {
        loadedCorrectly = loader.LoadBinaryFromFile(tinyModel.get(), &err, &warn, gltfPath.string());
    } else if (filePath.ends_with(".gltf")) {
        loadedCorrectly = loader.LoadASCIIFromFile(tinyModel.get(), &err, &warn, gltfPath.string());
    } else {
        fmt::print("Cannot load this type of model file\n");
        loadedCorrectly = false;
    }

    if (!warn.empty()) {
        fmt::print("Warn: {}\n", warn.c_str());
    }

    if (!err.empty()) {
        fmt::print("Err: {}\n", err.c_str());
    }

    if (!loadedCorrectly) {
        fmt::print("Failed to parse glTF\n");
        exit(-1);
    }

    updateMeshData(tinyModel, outputModel, override_color_with_normal);

    outputModel.textures.reserve(tinyModel->textures.size());
    for(uint32_t i = 0; i < tinyModel->textures.size(); i++) {
        tinygltf::Texture& texture = tinyModel->textures[i];
        tinygltf::Image& image = tinyModel->images[texture.source];

        outputModel.textures.emplace_back(std::move(std::make_unique<ModelImageData>(image.image, image.width,  image.height, image.component)));
    }

    updateMaterials(tinyModel, outputModel);

    return outModel;
}

void GLTFLoader::updateMeshData(std::shared_ptr<tinygltf::Model> tinyModel, Model& outputModel, bool override_color_with_normal) {

    uint32_t firstIndex = 0;
    uint32_t firstInstance = 0;

    for(size_t nodeIndex = 0; nodeIndex < tinyModel->nodes.size(); nodeIndex++) {

        // Confirm this is a MESH node
        if(tinyModel->nodes[nodeIndex].mesh == -1) {
            continue;
        }

        uint32_t meshId = tinyModel->nodes[nodeIndex].mesh;
        tinygltf::Mesh mesh = tinyModel->meshes[meshId];

        fmt::print("meshId: {}\n", meshId);

        Mesh currentMesh;
        currentMesh.name = mesh.name;

        glm::mat4 m(1.0);
        if(tinyModel->nodes[nodeIndex].matrix.size() == 16) {
            // use matrix attribute
            for(int i = 0; i < 4; i++) {
                for(int j = 0; j < 4; j++) {
                    m[i][j] = tinyModel->nodes[i].matrix[(i * 4) + j];
                }
            }
        } else {
            // use Translate x Rotate x Scale order
            glm::mat4 mScale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
            if(!tinyModel->nodes[nodeIndex].scale.empty()) {
                mScale = glm::scale(glm::mat4(1.0f), glm::vec3(tinyModel->nodes[nodeIndex].scale[0], tinyModel->nodes[nodeIndex].scale[1], tinyModel->nodes[nodeIndex].scale[2]));
            }

            glm::mat4 mRot = glm::mat4_cast(glm::quat(1, 0, 0, 0));
            if(!tinyModel->nodes[nodeIndex].rotation.empty()) {
                mRot = glm::mat4_cast(glm::quat(tinyModel->nodes[nodeIndex].rotation[3], tinyModel->nodes[nodeIndex].rotation[0], tinyModel->nodes[nodeIndex].rotation[1], tinyModel->nodes[nodeIndex].rotation[2]));
            }

            glm::mat4 mTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
            if(!tinyModel->nodes[nodeIndex].translation.empty()) {
                mTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(tinyModel->nodes[nodeIndex].translation[0], tinyModel->nodes[nodeIndex].translation[1], tinyModel->nodes[nodeIndex].translation[2]));
            }

            m = mTranslate * mRot * mScale;
        }

        // first index of this primitive
        firstIndex = 0;

        // Process the primitive's data, placing the data in Mesh object
        for(auto& primitive : mesh.primitives) {
            int positionAccessorIndex;
            int normalAccessorIndex;
            int tangentAccessorIndex;
            int uvAccessorIndex;
            int uvAccessorIndex2;

            if(primitive.material != -1) {
                currentMesh.material = primitive.material;
            }

            auto positionIt = primitive.attributes.find("POSITION");
            auto normalIt = primitive.attributes.find("NORMAL");

            if(positionIt == primitive.attributes.end() ||
                normalIt == primitive.attributes.end()) {
                fmt::print("primitive does not have positionIt or normalIt\n");
                continue;
            }

            positionAccessorIndex = positionIt->second;
            normalAccessorIndex = normalIt->second;

            auto tangentIt = primitive.attributes.find("TANGENT");
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            auto uv2It = primitive.attributes.find("TEXCOORD_1");

            bool hasTangent = tangentIt != primitive.attributes.end();
            bool hasUV = uvIt != primitive.attributes.end();
            bool hasUV2 = uv2It != primitive.attributes.end();

            if(hasTangent) {
                tangentAccessorIndex = tangentIt->second;
            }

            if(hasUV) {
                uvAccessorIndex = uvIt->second;
            }

            if(hasUV2) {
                uvAccessorIndex2 = uv2It->second;
            }

            if(primitive.indices == -1) {
                fmt::print("primitive does not have indices index\n");
                continue;
            }

            tinygltf::Accessor indicesAccessor = tinyModel->accessors[primitive.indices];
            tinygltf::Accessor positionAccessor = tinyModel->accessors[positionAccessorIndex];
            tinygltf::Accessor normalAccessor = tinyModel->accessors[normalAccessorIndex];

            const tinygltf::BufferView& indicesBufferView = tinyModel->bufferViews[indicesAccessor.bufferView];
            const tinygltf::Buffer& indicesBuffer = tinyModel->buffers[indicesBufferView.buffer];

            // indicesBuffer.data

            if(indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                // short - 16 bits / 2 bytes

                fmt::print("unsigned short indices accessor\n");

                ASSERT(indicesBuffer.data.size() % 2 == 0, "buffer indices not correct size for unsigned shorts");
                currentMesh.indices.reserve(indicesAccessor.count * 2);

//                for(uint32_t i = 0; i < indicesBuffer.data.size(); i += 2) {
//                    unsigned short currIndex = (static_cast<unsigned short>(indicesBuffer.data[i]) << 8) | (static_cast<unsigned short>(indicesBuffer.data[i + 1]));
//                    fmt::print("In-func index {}\n", currIndex);
//                    currentMesh.indices.push_back(static_cast<uint32_t>(currIndex));
//                }

//                for(uint32_t i = 0; i < indicesBuffer.data.size(); i += 2) {
//                    unsigned short currIndex = (static_cast<unsigned short>(indicesBuffer.data[i]) << 8) | (static_cast<unsigned short>(indicesBuffer.data[i + 1]));
//                    fmt::print("In-func index {}\n", currIndex);
//                    currentMesh.indices.push_back(static_cast<uint32_t>(currIndex));
//                }

                const uint16_t* convertedData = reinterpret_cast<const uint16_t *>(&indicesBuffer.data[indicesAccessor.byteOffset + indicesBufferView.byteOffset]);
                for(uint32_t i = 0; i < indicesAccessor.count; i++) {
                    currentMesh.indices.push_back(static_cast<uint32_t>(convertedData[i]));
                }

            } else if(indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                // int - 32 bits / 4 bytes

                fmt::print("unsigned int indices accessor\n");

                // ASSERT(indicesAccessor.count % 4 == 0, "buffer indices not correct size for unsigned ints");
                currentMesh.indices.reserve(indicesAccessor.count);

//                for(uint32_t i = 0; i < indicesBuffer.data.size(); i += 4) {
//                    unsigned int value = (static_cast<unsigned int>(indicesBuffer.data[i]) << 24) |
//                                         (static_cast<unsigned short>(indicesBuffer.data[i + 1]) << 16) |
//                                         (static_cast<unsigned short>(indicesBuffer.data[i + 2]) << 8) |
//                                         (static_cast<unsigned short>(indicesBuffer.data[i + 3]));
//
//                    currentMesh.indices.push_back(value);
//                }

                const uint32_t* convertedData = reinterpret_cast<const uint32_t *>(&indicesBuffer.data[indicesAccessor.byteOffset + indicesBufferView.byteOffset]);
                for(uint32_t i = 0; i < indicesAccessor.count; i++) {
                    currentMesh.indices.push_back(static_cast<uint32_t>(convertedData[i]));
                }

            }

            ASSERT(positionAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT &&
                   normalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "position and normal accessors must be floats");

            const tinygltf::BufferView& positionBufView = tinyModel->bufferViews[positionAccessor.bufferView];
            const tinygltf::Buffer& positionBuf = tinyModel->buffers[positionBufView.buffer];
            std::vector<float> positions; //  = uCharVecToFloatVec(positionBuf.data);
            const float* convertedPositionFloatData = reinterpret_cast<const float*>(&positionBuf.data[positionAccessor.byteOffset + positionBufView.byteOffset]);
            for(uint32_t i = 0; i < positionAccessor.count; i++) {
                int index = i * 3;
                positions.push_back(static_cast<float>(convertedPositionFloatData[index]));
                positions.push_back(static_cast<float>(convertedPositionFloatData[index + 1]));
                positions.push_back(static_cast<float>(convertedPositionFloatData[index + 2]));
            }
            // fmt::print("{}\n", fmt::join(positions, ", "));

            const tinygltf::BufferView& normalBufView = tinyModel->bufferViews[normalAccessor.bufferView];
            const tinygltf::Buffer& normalBuf = tinyModel->buffers[normalBufView.buffer];
            std::vector<float> normals;
            const float* convertedNormalFloatData = reinterpret_cast<const float*>(&normalBuf.data[normalAccessor.byteOffset + normalBufView.byteOffset]);
            for(uint32_t i = 0; i < normalAccessor.count; i++) {
                int index = i * 3;
                normals.push_back(static_cast<float>(convertedNormalFloatData[index]));
                normals.push_back(static_cast<float>(convertedNormalFloatData[index + 1]));
                normals.push_back(static_cast<float>(convertedNormalFloatData[index + 2]));
            }
            // std::vector<float> normals = uCharVecToFloatVec(normalBuf.data);

            auto vertexCount = positionAccessor.count;

            std::vector<float> tangents(vertexCount * 4, 0.0);
            std::vector<float> uvs(vertexCount * 2, 0.0);
            std::vector<float> uv2s(vertexCount * 2, 0.0);

            tinygltf::Accessor tangentAccessor;
            if(hasTangent) {
                // TODO: check tangent accessor's component type??
                tangentAccessor = tinyModel->accessors[tangentIt->second];
                const tinygltf::BufferView& tangentBufView = tinyModel->bufferViews[tangentAccessor.bufferView];
                const tinygltf::Buffer& tangentBuf = tinyModel->buffers[tangentBufView.buffer];
                // tangents = uCharVecToFloatVec(tangentBuf.data);

                const float* convertedTangentFloatData = reinterpret_cast<const float*>(&tangentBuf.data[tangentAccessor.byteOffset + tangentBufView.byteOffset]);
                for(uint32_t i = 0; i < tangentAccessor.count; i++) {
                    int index = i * 4;
                    tangents[index] = static_cast<float>(convertedTangentFloatData[index]);
                    tangents[index + 1] = static_cast<float>(convertedTangentFloatData[index + 1]);
                    tangents[index + 2] = static_cast<float>(convertedTangentFloatData[index + 2]);
                    tangents[index + 3] = static_cast<float>(convertedTangentFloatData[index + 3]);
                }
            }

            tinygltf::Accessor uvAccessor;
            if(hasUV) {
                uvAccessor = tinyModel->accessors[uvIt->second];
                const tinygltf::BufferView& uvBufView = tinyModel->bufferViews[uvAccessor.bufferView];
                const tinygltf::Buffer& uvBuf = tinyModel->buffers[uvBufView.buffer];
                // uvs = uCharVecToFloatVec(uvBuf.data);

                const float* convertedUVFloatData = reinterpret_cast<const float*>(&uvBuf.data[uvAccessor.byteOffset + uvBufView.byteOffset]);
                for(uint32_t i = 0; i < uvAccessor.count; i++) {
                    int index = i * 2;
                    uvs[index] = static_cast<float>(convertedUVFloatData[index]);
                    uvs[index + 1] = static_cast<float>(convertedUVFloatData[index + 1]);
                }
            }

            tinygltf::Accessor uv2Accessor;
            if(hasUV2) {
                uv2Accessor = tinyModel->accessors[uv2It->second];
                const tinygltf::BufferView& uv2BufView = tinyModel->bufferViews[uv2Accessor.bufferView];
                const tinygltf::Buffer& uv2Buf = tinyModel->buffers[uv2BufView.buffer];
                // uv2s = uCharVecToFloatVec(uv2Buf.data);

                const float* convertedUV2FloatData = reinterpret_cast<const float*>(&uv2Buf.data[uv2Accessor.byteOffset + uv2BufView.byteOffset]);
                for(uint32_t i = 0; i < uv2Accessor.count; i++) {
                    int index = i * 2;
                    uv2s[index] = static_cast<float>(convertedUV2FloatData[index]);
                    uv2s[index + 1] = static_cast<float>(convertedUV2FloatData[index + 1]);
                }
            }

            for(uint64_t i = 0; i < vertexCount; i++) {
                const std::array<uint64_t, 4> fourElementsIndices {
                    4*i, (4*i) + 1, (4*i) + 2, (4*i) + 3
                };

                const std::array<uint64_t, 4> threeElementsIndices {
                    3*i, (3*i) + 1, (3*i) + 2
                };

                const std::array<uint64_t, 4> twoElementsIndices {
                    2*i, (2*i) + 1
                };

                Vertex vertex {
                    .pos = glm::vec3(positions[threeElementsIndices[0]],
                                     positions[threeElementsIndices[1]],
                                     positions[threeElementsIndices[2]]),
                    .normal = glm::vec3(normals[threeElementsIndices[0]],
                                     normals[threeElementsIndices[1]],
                                     normals[threeElementsIndices[2]]),
                    .tangent = glm::vec4(tangents[fourElementsIndices[0]],
                                         tangents[fourElementsIndices[1]],
                                         tangents[fourElementsIndices[2]],
                                         tangents[fourElementsIndices[3]]),
                    .color = glm::vec4(1.0, 1.0, 1.0, 1.0),
                    .texCoord = glm::vec2(uvs[twoElementsIndices[0]],
                                          uvs[twoElementsIndices[1]]),
                    .texCoord1 = glm::vec2(uv2s[twoElementsIndices[0]],
                                           uv2s[twoElementsIndices[1]]),
                    .material = uint32_t(currentMesh.material)
                };

                if(override_color_with_normal) {
                    vertex.color = glm::vec4(normals[threeElementsIndices[0]],
                                             normals[threeElementsIndices[1]],
                                             normals[threeElementsIndices[2]],
                                             1.0f);
                }

                vertex.applyTransform(m);

                currentMesh.vertices.emplace_back(vertex);

                if (vertex.pos.x < currentMesh.minAABB.x) {
                    currentMesh.minAABB.x = vertex.pos.x;
                }
                if (vertex.pos.y < currentMesh.minAABB.y) {
                    currentMesh.minAABB.y = vertex.pos.y;
                }
                if (vertex.pos.z < currentMesh.minAABB.z) {
                    currentMesh.minAABB.z = vertex.pos.z;
                }

                if (vertex.pos.x > currentMesh.maxAABB.x) {
                    currentMesh.maxAABB.x = vertex.pos.x;
                }
                if (vertex.pos.y > currentMesh.maxAABB.y) {
                    currentMesh.maxAABB.y = vertex.pos.y;
                }
                if (vertex.pos.z > currentMesh.maxAABB.z) {
                    currentMesh.maxAABB.z = vertex.pos.z;
                }
            };
        }

        if(currentMesh.indices.empty() || currentMesh.vertices.empty()) {
            continue;
        }

        IndirectDrawDataAndMeshData indirectDrawData {
            .indexCount = uint32_t(currentMesh.indices.size()),
            .instanceCount = 1,
            .firstIndex = firstIndex,
            .vertexOffset = firstInstance,
            .firstInstance = 0,
            .meshId = uint32_t(outputModel.meshes.size()),
            .materialId = static_cast<int>(currentMesh.material)
        };

        firstIndex += currentMesh.indices.size();
        firstInstance += currentMesh.vertices.size();

        currentMesh.extents = (currentMesh.maxAABB - currentMesh.minAABB) * 0.5f;
        currentMesh.center = currentMesh.minAABB + currentMesh.extents;
        outputModel.meshes.emplace_back(std::move(currentMesh));
        outputModel.indirectDrawDataSet.emplace_back(std::move(indirectDrawData));
        outputModel.totalVertexSize += sizeof(Vertex) * outputModel.meshes.back().vertices.size();
        outputModel.totalIndexSize += sizeof(Mesh::Indices) * outputModel.meshes.back().indices.size();
    };

}

std::vector<float> GLTFLoader::uCharVecToFloatVec(const std::vector<unsigned char>& rawData) {
    ASSERT(rawData.size() % 4 == 0, "raw buffer must be a size that's a multiple of 4");

    std::vector<float> floats;
    floats.reserve(rawData.size() / 4);

    for(uint32_t i = 0; i < rawData.size(); i += 4) {
        float value;
        std::memcpy(&value, &rawData[i], sizeof(value));
        floats.push_back(value);
    }

    return floats;
}

void GLTFLoader::updateMaterials(std::shared_ptr<tinygltf::Model> tinyModel, Model& outputModel) {

    for(auto& mat : tinyModel->materials) {
        GLTFMaterialData currentMaterial;

        auto data = {
            std::pair{
                &mat.pbrMetallicRoughness.baseColorTexture.index,
                &currentMaterial.baseColorTextureId
            },
            std::pair{
                &mat.pbrMetallicRoughness.metallicRoughnessTexture.index,
                &currentMaterial.metallicRoughnessTextureId
            },
            std::pair{
                &mat.normalTexture.index,
                &currentMaterial.normalTextureTextureId
            },
            std::pair{
                &mat.emissiveTexture.index,
                &currentMaterial.emissiveTextureId
            }
        };

        for(auto& d : data) {
            if(*d.first != -1) {
                *d.second = *d.first;
            }
        }

        currentMaterial.baseColorSamplerId = 0;
        currentMaterial.baseColor = glm::vec4 (
            mat.pbrMetallicRoughness.baseColorFactor[0], mat.pbrMetallicRoughness.baseColorFactor[1],
            mat.pbrMetallicRoughness.baseColorFactor[2], mat.pbrMetallicRoughness.baseColorFactor[3]
        );

        currentMaterial.metallicFactor = mat.pbrMetallicRoughness.metallicFactor;
        currentMaterial.roughnessFactor = mat.pbrMetallicRoughness.roughnessFactor;

        outputModel.materials.emplace_back(std::move(currentMaterial));
    };

}
 * void convertModelToOneMeshPerBuffer_VerticesIndicesMaterialsAndTextures(const Context& context, CommandQueueManager& queueManager,
                                                                        VkCommandBuffer commandBuffer, const Model& model,
                                                                        std::vector<std::shared_ptr<Buffer>>& buffers,
                                                                        std::vector<std::shared_ptr<Texture>>& textures,
                                                                        std::vector<std::shared_ptr<Sampler>>& samplers) {

    convertModelToOneMeshPerBuffer_VerticesIndicesAndMaterials(context, queueManager, commandBuffer, model, buffers, samplers);

    for(size_t textureIndex = 0; const auto& texture : model.textures) {

        textures.emplace_back(context.createTexture(
            VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VkExtent3D {
                .width = static_cast<uint32_t>(texture->width),
                .height = static_cast<uint32_t>(texture->height),
                .depth = 1u,
            },
            1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true,
            VK_SAMPLE_COUNT_1_BIT, std::to_string(textureIndex)
        ));



        // Upload texture to GPU
        auto textureUploadStagingBuffer = context.createStagingBuffer(textures.back()->deviceSize(), std::to_string(textureIndex));

        textures.back()->uploadAndGenerateMips(commandBuffer, textureUploadStagingBuffer.get(), texture->data);

        queueManager.disposeWhenSubmitCompletes(std::move(textureUploadStagingBuffer));

        textureIndex++;
    }

}*/

/*void convertModelToOneMeshPerBuffer_VerticesIndicesAndMaterials(,
                                                                VkCommandBuffer commandBuffer, const Model& model,
                                                                std::vector<std::shared_ptr<Buffer>>& buffers,
                                                                std::vector<std::shared_ptr<Sampler>>& samplers) {

    for(size_t meshIndex = 0; const auto& mesh : model.meshes) {
        const auto verticesSize = sizeof(Vertex) * mesh.vertices.size();

        // Create vertex buffer
        buffers.emplace_back(context.createBuffer(
            verticesSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            #if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
            // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            #endif
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            "Mesh " + std::to_string(meshIndex) + " vertex buffer"
        ));

        // Upload vertex buffer
        context.uploadToGPUBuffer(queueManager, commandBuffer, buffers.back().get(), reinterpret_cast<const void*>(mesh.vertices.data()), verticesSize);

        // Create index buffer
        const auto indicesSize = sizeof(uint32_t) * mesh.indices.size();
        buffers.emplace_back(context.createBuffer(
            indicesSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            #if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
            // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            #endif
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            "Mesh " + std::to_string(meshIndex) + " index buffer"));

        // Upload index buffer
        context.uploadToGPUBuffer(queueManager, commandBuffer, buffers.back().get(),
                                  reinterpret_cast<const void*>(mesh.indices.data()),
                                  indicesSize);

        meshIndex++;
    }

    const auto totalMaterialSize = sizeof(GLTFMaterialData) * model.materials.size();

    buffers.emplace_back(context.createBuffer(
        totalMaterialSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        #if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
        // | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        #endif
        ,
        VMA_MEMORY_USAGE_GPU_ONLY, "materials"));

    context.uploadToGPUBuffer(queueManager, commandBuffer, buffers.back().get(),
                              reinterpret_cast<const void*>(model.materials.data()),
                              totalMaterialSize);
}*/
