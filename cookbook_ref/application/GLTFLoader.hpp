//
// Created by darby on 7/17/2024.
//

#pragma once

#include "tiny_gltf.h"
#include "Model.hpp"

namespace Fairy::Application {

class GLTFLoader {

public:
    std::shared_ptr<Graphics::Model> load(const std::string& filePath);


private:
    void updateMeshData(std::shared_ptr<tinygltf::Model> tinyModel, Graphics::Model& outputModel);
    void updateMaterials(std::shared_ptr<tinygltf::Model> tinyModel, Graphics::Model& outputModel);

    std::vector<float> uCharVecToFloatVec(const std::vector<unsigned char>& rawData);

};

// Produces one vertex buffer and one index buffer for each mesh, plus a buffer for the scene materials. The buffers are
// interleaved: V0 I0 V1 I1 ... Vn In
void convertModelToOneMeshPerBuffer_VerticesIndicesMaterialsAndTextures(
    const Graphics::Context& context, Graphics::CommandQueueManager& queueManager,
    VkCommandBuffer commandBuffer, const Graphics::Model& model,
    std::vector<std::shared_ptr<Graphics::Buffer>>& buffers,
    std::vector<std::shared_ptr<Graphics::Texture>>& textures,
    std::vector<std::shared_ptr<Graphics::Sampler>>& samplers
);

void convertModelToOneMeshPerBuffer_VerticesIndicesAndMaterials(
    const Graphics::Context& context, Graphics::CommandQueueManager& queueManager,
    VkCommandBuffer commandBuffer, const Graphics::Model& model,
    std::vector<std::shared_ptr<Graphics::Buffer>>& buffers,
    std::vector<std::shared_ptr<Graphics::Sampler>>& samplers
);

}