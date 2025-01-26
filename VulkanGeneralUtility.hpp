//
// Created by darby on 1/23/2025.
//

#include "Common.hpp"
#include "GraphicsTypes.hpp"
#include "ImmediateSubmitCommandBuffer.hpp"
#include "Buffer.hpp"

namespace vk_util {

//    GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices, const std::string& mesh_name = "");
    GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices, VmaAllocator allocator, VkDevice device, ImmediateSubmitCommandBuffer& immediate_submit_command_buffer, const std::string& mesh_name);

}
