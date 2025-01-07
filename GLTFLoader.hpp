//
// Created by darby on 7/17/2024.
//

#pragma once

#include "Common.hpp"
#include "GraphicsTypes.hpp"
#include "tiny_gltf.h"

class Engine;

class GLTFLoader {

public:
    std::shared_ptr<Model> load(const std::string& filePath, bool override_color_with_normal);


private:
    void updateMeshData(std::shared_ptr<tinygltf::Model> tinyModel, Model& outputModel, bool override_color_with_normal);
    void updateMaterials(std::shared_ptr<tinygltf::Model> tinyModel, Model& outputModel);

    std::vector<float> uCharVecToFloatVec(const std::vector<unsigned char>& rawData);

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
