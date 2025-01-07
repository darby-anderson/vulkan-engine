//
// Created by darby on 12/30/2024.
//

#pragma once

#include "Common.hpp"

namespace vk_debug {

template <typename T>
void name_resource(VkDevice device, VkObjectType object_type, T handle, const char* name) {
    fmt::print("Setting object name to {}\n", name);

    VkDebugUtilsObjectNameInfoEXT name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = object_type,
            .objectHandle = reinterpret_cast<uint64_t>(handle),
            .pObjectName = name
    };

    vkSetDebugUtilsObjectNameEXT(device, &name_info);
}

}
