//
// Created by darby on 1/23/2025.
//

#include "VulkanGeneralUtility.hpp"

namespace vk_util {

    /*
     * Uploads a mesh's vertex and index buffers to the GPU
     */
    GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices, VmaAllocator allocator, VkDevice device, ImmediateSubmitCommandBuffer& immediate_submit_command_buffer, const std::string& mesh_name) {

        const size_t vertex_buffer_size = sizeof(Vertex) * vertices.size();
        const size_t index_buffer_size = sizeof(uint32_t) * indices.size();

        GPUMeshBuffers new_surface;
        new_surface.vertex_buffer.init(allocator, vertex_buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                       VMA_MEMORY_USAGE_GPU_ONLY);
        new_surface.vertex_buffer.set_name(device, (mesh_name + std::string(" vertex buffer")).c_str());

        VkBufferDeviceAddressInfo device_address_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = new_surface.vertex_buffer.buffer
        };

        new_surface.vertex_buffer_address = vkGetBufferDeviceAddress(device, &device_address_info);

        new_surface.index_buffer.init(allocator, index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VMA_MEMORY_USAGE_GPU_ONLY);
        new_surface.index_buffer.set_name(device, (mesh_name + std::string(" index buffer")).c_str());

        Buffer staging_buffer;
        staging_buffer.init(allocator, vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        staging_buffer.set_name(device, (mesh_name + std::string(" Index/Vertex Staging Buffer")).c_str());

//        void* data = staging_buffer.allocation->GetMappedData();
        void* data = staging_buffer.info.pMappedData;

        // copy vertex buffer data
        memcpy(data, vertices.data(), vertex_buffer_size);
        memcpy((char*)data + vertex_buffer_size, indices.data(), index_buffer_size);

        immediate_submit_command_buffer.submit([&](VkCommandBuffer cmd) {
            VkBufferCopy vertex_copy {
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = vertex_buffer_size
            };

            vkCmdCopyBuffer(cmd, staging_buffer.buffer, new_surface.vertex_buffer.buffer, 1, &vertex_copy);

            VkBufferCopy index_copy {
                    .srcOffset = vertex_buffer_size,
                    .dstOffset = 0,
                    .size = index_buffer_size
            };

            vkCmdCopyBuffer(cmd, staging_buffer.buffer, new_surface.index_buffer.buffer, 1, &index_copy);
        });

        staging_buffer.destroy_buffer();

        return new_surface;
    }

}
