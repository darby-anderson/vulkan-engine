//
// Created by darby on 5/28/2024.
//
#pragma once

#include <iostream>
#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>
#include <unordered_set>
#include <vector>

#define VK_CHECK(func) \
    {                  \
        const VkResult result = func; \
        if(result != VK_SUCCESS) {    \
            std::cerr << "Error calling function " << #func \
                << " at " << __FILE__ << ":"                    \
                << __LINE__ << ". Result is "                   \
                << string_VkResult(result) \
                << std::endl;             \
            assert(false);               \
        }\
    }

namespace Fairy::Graphics {

    std::unordered_set<std::string> filterExtensions(std::vector<std::string> availableExtensions, std::vector<std::string> requestedExtensions);

    VkImageViewType imageTypeToImageViewType(VkImageType imageType, VkImageCreateFlags flags, bool multiview);

    uint32_t bytesPerPixel(VkFormat format);

}