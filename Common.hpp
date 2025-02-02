//
// Created by darby on 11/21/2024.
//

#pragma once

#define NOMINMAX // required before including windows.h. It will create a max() macro that occludes numeric_limit's max() function

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <functional>
#include <deque>
#include <fstream>
#include <array>

#include <fmt/core.h>
#include <fmt/ranges.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE // necessary when using glm::ortho, as we define a 0->1 range for our depth range
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtx/type_aligned.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#define VK_USE_PLATFORM_WIN32_KHR
#include "volk.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0 // necessary since we load via volk
#include "vk_mem_alloc.h"

#undef VK_USE_PLATFORM_WIN32_KHR // undef and redef due to an issue with fullscreen extension
#include <vulkan/vk_enum_string_helper.h>
#define VK_USE_PLATFORM_WIN32_KHR

#define VK_CHECK(func) \
    {                  \
        const VkResult result = func; \
        if(result != VK_SUCCESS) {    \
            fmt::print("Error calling function {} at {}:{}. Result is {}\n", #func, __FILE__, __LINE__, string_VkResult(result));   \
            assert(false);               \
        } \
    }

#define ASSERT(condition, message) \
    do { \
        if (! (condition)) {          \
            fmt::print("Assertion `{}` failed in {} line {}. Message: {}\n", #condition, __FILE__, __LINE__, message);                          \
            std::terminate(); \
        } \
    } while (false)



