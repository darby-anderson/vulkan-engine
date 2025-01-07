//
// Created by darby on 5/30/2024.
//

#pragma once

#include <cassert>
#include <vector>
#include <string>

const bool useDebugBreak = true;

#define ASSERT(expr, message)       \
    {                               \
        void(message);              \
        if(useDebugBreak && !(expr)) { \
            __debugbreak();         \
        }                           \
        assert(expr);               \
    }

#define MOVABLE_ONLY(CLASS_NAME)                     \
  CLASS_NAME(const CLASS_NAME&) = delete;            \
  CLASS_NAME& operator=(const CLASS_NAME&) = delete; \
  CLASS_NAME(CLASS_NAME&&) noexcept = default;       \
  CLASS_NAME& operator=(CLASS_NAME&&) noexcept = default;

#define LOGE(format, ...)                 \
  do {                                    \
    fprintf(stderr, format, __VA_ARGS__); \
    fprintf(stderr, "\n\n");              \
    fflush(stderr);                       \
  } while (0)
#define LOGW(format, ...) LOGE(format, __VA_ARGS__)
#define LOGI(format, ...) LOGE(format, __VA_ARGS__)
#define LOGD(format, ...) LOGE(format, __VA_ARGS__)

namespace Fairy::Core {

    int endsWith(const char* s, const char* part);
    std::vector<char> readFile(const std::string& filePath, bool isBinary);

}
