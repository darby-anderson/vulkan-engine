//
// Created by darby on 12/16/2024.
//

#pragma once

#include "Common.hpp"

namespace vk_file {

bool load_shader_module(const char* file_path, VkDevice device, VkShaderModule* outModule);

}