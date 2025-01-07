//
// Created by darby on 7/17/2024.
//
#include <iostream>

#include "volk.h"

#include "Context.hpp"
#include "GLTFLoader.hpp"
#include "Model.hpp"
#include "CoreUtils.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"
#include "Sampler.hpp"
#include "CommandQueueManager.hpp"

// #define STB_IMAGE_IMPLEMENTATION <- defined in Model.cpp
// #define STB_IMAGE_WRITE_IMPLEMENTATION <- defined in Model.cpp
#define TINYGLTF_IMPLEMENTATION // need to define this after including GLTFLoader.hpp since it also includes tiny_gltf
#include "tiny_gltf.h"

namespace {
    std::vector<unsigned char> imageData(std::shared_ptr<tinygltf::Model> tinyModel, uint32_t imageId) {
        tinygltf::Image& image = tinyModel->images[imageId];
        return image.image;
    }
}

namespace Fairy::Application {

std::shared_ptr<Graphics::Model> GLTFLoader::load(const std::string& filePath) {

    std::shared_ptr<Graphics::Model> outModel = std::make_shared<Graphics::Model>();
    Graphics::Model& outputModel = *outModel.get();

    auto gltfPath = std::filesystem::current_path();
    gltfPath /= filePath;

    tinygltf::TinyGLTF loader;
    std::shared_ptr<tinygltf::Model> tinyModel = std::make_shared<tinygltf::Model>();
    std::string err;
    std::string warn;

    bool loadedCorrectly = loader.LoadASCIIFromFile(tinyModel.get(), &err, &warn, gltfPath.string());

    if (!warn.empty()) {
        printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        printf("Err: %s\n", err.c_str());
    }

    if (!loadedCorrectly) {
        printf("Failed to parse glTF\n");
        exit(-1);
    }

    updateMeshData(tinyModel, outputModel);

    outputModel.textures.reserve(tinyModel->textures.size());
    for(uint32_t i = 0; i < tinyModel->textures.size(); i++) {
        tinygltf::Texture& texture = tinyModel->textures[i];
        tinygltf::Image& image = tinyModel->images[texture.source];

        outputModel.textures.emplace_back(std::move(std::make_unique<Graphics::ModelImageData>(image.image, image.width,  image.height, image.component)));
    }

    updateMaterials(tinyModel, outputModel);

    return outModel;
}

void GLTFLoader::updateMeshData(std::shared_ptr<tinygltf::Model> tinyModel, Graphics::Model& outputModel) {

    uint32_t firstIndex = 0;
    uint32_t firstInstance = 0;

    for(size_t nodeIndex = 0; nodeIndex < tinyModel->nodes.size(); nodeIndex++) {

        // Confirm this is a MESH node
        if(tinyModel->nodes[nodeIndex].mesh == -1) {
            continue;
        }

        uint32_t meshId = tinyModel->nodes[nodeIndex].mesh;
        tinygltf::Mesh mesh = tinyModel->meshes[meshId];
        Graphics::Mesh currentMesh;

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

            m = mTranslate * mRot * mScale; // TODO: don't we need to rotate??
        }

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
                std::cout << "primitive does not have positionIt or normalIt" << std::endl;
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
                std::cout << "primitive does not have indices index" << std::endl;
                continue;
            }

            tinygltf::Accessor indicesAccessor = tinyModel->accessors[primitive.indices];
            tinygltf::Accessor positionAccessor = tinyModel->accessors[positionAccessorIndex];
            tinygltf::Accessor normalAccessor = tinyModel->accessors[normalAccessorIndex];

            const tinygltf::BufferView& bufferView = tinyModel->bufferViews[indicesAccessor.bufferView];
            const tinygltf::Buffer& buffer = tinyModel->buffers[bufferView.buffer];

            if(indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                // short - 16 bits / 2 bytes

                ASSERT(buffer.data.size() % 2 == 0, "buffer indices not correct size for unsigned shorts");
                currentMesh.indices.reserve(buffer.data.size() / 2);

                for(uint32_t i = 0; i < buffer.data.size(); i += 2) {
                    unsigned short currIndex = (static_cast<unsigned short>(buffer.data[i]) << 8) | (static_cast<unsigned short>(buffer.data[i + 1]));
                    currentMesh.indices.push_back(static_cast<uint32_t>(currIndex));
                }
            } else if(indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                // int - 32 bits / 4 bytes

                ASSERT(buffer.data.size() % 4 == 0, "buffer indices not correct size for unsigned ints");
                currentMesh.indices.reserve(buffer.data.size() / 4);

                for(uint32_t i = 0; i < buffer.data.size(); i += 4) {
                    unsigned int value = (static_cast<unsigned int>(buffer.data[i]) << 24) |
                                         (static_cast<unsigned short>(buffer.data[i + 1]) << 16) |
                                         (static_cast<unsigned short>(buffer.data[i + 2]) << 8) |
                                         (static_cast<unsigned short>(buffer.data[i + 3]));

                    currentMesh.indices.push_back(value);
                }
            }

            ASSERT(positionAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT &&
                   normalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "position and normal accessors must be floats");

            const tinygltf::BufferView& positionBufView = tinyModel->bufferViews[positionAccessor.bufferView];
            const tinygltf::Buffer& positionBuf = tinyModel->buffers[positionBufView.buffer];
            std::vector<float> positions = uCharVecToFloatVec(positionBuf.data);

            const tinygltf::BufferView& normalBufView = tinyModel->bufferViews[normalAccessor.bufferView];
            const tinygltf::Buffer& normalBuf = tinyModel->buffers[normalBufView.buffer];
            std::vector<float> normals = uCharVecToFloatVec(normalBuf.data);

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
                tangents = uCharVecToFloatVec(tangentBuf.data);
            }

            tinygltf::Accessor uvAccessor;
            if(hasUV) {
                uvAccessor = tinyModel->accessors[uvIt->second];
                const tinygltf::BufferView& uvBufView = tinyModel->bufferViews[uvAccessor.bufferView];
                const tinygltf::Buffer& uvBuf = tinyModel->buffers[uvBufView.buffer];
                uvs = uCharVecToFloatVec(uvBuf.data);
            }

            tinygltf::Accessor uv2Accessor;
            if(hasUV2) {
                uv2Accessor = tinyModel->accessors[uv2It->second];
                const tinygltf::BufferView& uv2BufView = tinyModel->bufferViews[uv2Accessor.bufferView];
                const tinygltf::Buffer& uv2Buf = tinyModel->buffers[uv2BufView.buffer];
                uv2s = uCharVecToFloatVec(uv2Buf.data);
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

                Graphics::Vertex vertex {
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
                    .texCoord = glm::vec2(uvs[twoElementsIndices[0]],
                                          uvs[twoElementsIndices[1]]),
                    .texCoord1 = glm::vec2(uv2s[twoElementsIndices[0]],
                                           uv2s[twoElementsIndices[1]]),
                    .material = uint32_t(currentMesh.material)
                };

                vertex.applyTransform(m);

                currentMesh.vertices.emplace_back(vertex);
                currentMesh.vertices16Bit.emplace_back(to16BitVertex(vertex));

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

        Graphics::IndirectDrawDataAndMeshData indirectDrawData {
            .indexCount = uint32_t(currentMesh.indices.size()),
            .instanceCount = 1,
            .firstIndex = firstIndex,
            .vertexOffset = firstInstance,
            .firstInstance = 0,
            .meshId = uint32_t(outputModel.meshes.size()),
            .materialIndex = static_cast<int>(currentMesh.material)
        };

        firstIndex += currentMesh.indices.size();
        firstInstance += currentMesh.vertices.size();

        currentMesh.extents = (currentMesh.maxAABB - currentMesh.minAABB) * 0.5f;
        currentMesh.center = currentMesh.minAABB + currentMesh.extents;
        outputModel.meshes.emplace_back(std::move(currentMesh));
        outputModel.indirectDrawDataSet.emplace_back(std::move(indirectDrawData));
        outputModel.totalVertexSize += sizeof(Graphics::Vertex) * outputModel.meshes.back().vertices.size();
        outputModel.totalIndexSize += sizeof(Graphics::Mesh::Indices) * outputModel.meshes.back().indices.size();
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

void GLTFLoader::updateMaterials(std::shared_ptr<tinygltf::Model> tinyModel, Graphics::Model& outputModel) {

    for(auto& mat : tinyModel->materials) {
        Graphics::Material currentMaterial;

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
void convertModelToOneMeshPerBuffer_VerticesIndicesMaterialsAndTextures(const Graphics::Context& context, Graphics::CommandQueueManager& queueManager,
                                                                        VkCommandBuffer commandBuffer, const Graphics::Model& model,
                                                                        std::vector<std::shared_ptr<Graphics::Buffer>>& buffers,
                                                                        std::vector<std::shared_ptr<Graphics::Texture>>& textures,
                                                                        std::vector<std::shared_ptr<Graphics::Sampler>>& samplers) {

    Fairy::Application::convertModelToOneMeshPerBuffer_VerticesIndicesAndMaterials(context, queueManager, commandBuffer, model, buffers, samplers);

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

}

void convertModelToOneMeshPerBuffer_VerticesIndicesAndMaterials(const Graphics::Context& context, Graphics::CommandQueueManager& queueManager,
                                                                VkCommandBuffer commandBuffer, const Graphics::Model& model,
                                                                std::vector<std::shared_ptr<Graphics::Buffer>>& buffers,
                                                                std::vector<std::shared_ptr<Graphics::Sampler>>& samplers) {

    for(size_t meshIndex = 0; const auto& mesh : model.meshes) {
        const auto verticesSize = sizeof(Graphics::Vertex) * mesh.vertices.size();

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

        // Upload vertices
        context.uploadToGPUBuffer(queueManager, commandBuffer, buffers.back().get(), reinterpret_cast<const void*>(mesh.vertices.data()), verticesSize);

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

        // Upload indices
        context.uploadToGPUBuffer(queueManager, commandBuffer, buffers.back().get(),
                                  reinterpret_cast<const void*>(mesh.indices.data()),
                                  indicesSize);

        meshIndex++;
    }

    const auto totalMaterialSize = sizeof(Graphics::Material) * model.materials.size();

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
}

}

