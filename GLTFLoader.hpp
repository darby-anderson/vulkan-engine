//
// Created by darby on 7/17/2024.
//

#pragma once

#include "Common.hpp"
#include "GraphicsTypes.hpp"
#include "tiny_gltf.h"
#include "SceneGraphMembers.hpp"
#include "GLTFMetallic_Roughness.hpp"

class Engine;

class GLTFLoader {

public:
    // std::shared_ptr<Model> load(const std::string& filePath, bool override_color_with_normal);
    std::shared_ptr<GLTFFile> load_file(VkDevice device, VmaAllocator allocator, GLTFMetallic_Roughness& material_creator,
        ImmediateSubmitCommandBuffer& immediate_submit_command_buffer,
        AllocatedImage texture_load_error_image, VkSampler texture_load_error_sampler,
        const std::string& filePath, bool override_color_with_normal);

private:
//    void updateMeshData(std::shared_ptr<tinygltf::Model> tinyModel, Model& outputModel, bool override_color_with_normal);
//    void updateMaterials(std::shared_ptr<tinygltf::Model> tinyModel, Model& outputModel);
//
//    std::vector<float> uCharVecToFloatVec(const std::vector<unsigned char>& rawData);

};

// Produces one vertex buffer and one index buffer for each mesh, plus a buffer for the scene materials. The buffers are
// interleaved: V0 I0 V1 I1 ... Vn In
/*void convertModelToOneMeshPerBuffer_VerticesIndicesMaterialsAndTextures(

    VkCommandBuffer commandBuffer, const Model& model,
    std::vector<std::shared_ptr<Buffer>>& buffers,
    std::vector<std::shared_ptr<Texture>>& textures,
    std::vector<std::shared_ptr<Sampler>>& samplers
);*/

/*void convertModelToOneMeshPerBuffer_VerticesIndicesAndMaterials(

    VkCommandBuffer commandBuffer, const Model& model,
    std::vector<std::shared_ptr<Buffer>>& buffers,
    std::vector<std::shared_ptr<Sampler>>& samplers
);*/
