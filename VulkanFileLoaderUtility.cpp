//
// Created by darby on 12/19/2024.
//

#include "VulkanFileLoaderUtility.hpp"

namespace vk_file {

    bool load_shader_module(const char* file_path, VkDevice device, VkShaderModule* outModule) {

        std::ifstream file(file_path, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            return false;
        }

        // get the file size in bytes
        size_t file_size = (size_t) file.tellg();

        // get buffer we will load spirv into, in uint32s
        std::vector <uint32_t> buffer(file_size / sizeof(uint32_t));

        // read the file into the buffer
        file.seekg(0);
        file.read((char*) buffer.data(), file_size);
        file.close();

        VkShaderModuleCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .codeSize = buffer.size() * sizeof(uint32_t),
                .pCode = buffer.data(),
        };

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &create_info, nullptr, &shaderModule)) {
            return false;
        }
        *outModule = shaderModule;
        return true;
    }

    std::string extract_file_name_from_path(const char* file_path) {
        std::string path = std::string(file_path);
        int last_forward_slash = path.rfind('/'); // path.find_last_of('/');

        if(last_forward_slash == std::string::npos) {
            return "";
        }

        return path.substr(last_forward_slash + 1);
    }

}