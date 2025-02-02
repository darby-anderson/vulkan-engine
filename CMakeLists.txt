cmake_minimum_required(VERSION 3.20)
project(VulkanEngine)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED) #  SPIRV-Tools)

include(FetchContent)

# executable
add_executable(VulkanEngine main.cpp
        Engine.hpp
        Window.cpp
        Window.hpp
        Engine.cpp
        Common.hpp
        VulkanContext.cpp
        VulkanContext.hpp
        Device.cpp
        Device.hpp
        PhysicalDevice.cpp
        PhysicalDevice.hpp
        Swapchain.cpp
        Swapchain.hpp
        VulkanInitUtility.hpp
        VulkanInitUtility.cpp
        VulkanImageUtility.hpp
        AllocatedImage.hpp
        DescriptorLayoutBuilder.hpp
        DescriptorAllocatorGrowable.hpp
        DescriptorAllocatorGrowable.cpp
        ComputePipeline.cpp
        ComputePipeline.hpp
        DeletionQueue.hpp
        VulkanFileLoaderUtility.hpp
        ComputeEffect.hpp
        PipelineBuilder.cpp
        PipelineBuilder.hpp
        VulkanFileLoaderUtility.cpp
        Buffer.cpp
        Buffer.hpp
        GraphicsTypes.hpp
        GLTFLoader.cpp
        GLTFLoader.hpp
        VulkanDebugUtility.hpp
        DescriptorWriter.cpp
        DescriptorWriter.hpp
        AllocatedImage.cpp
        ImmediateSubmitCommandBuffer.cpp
        ImmediateSubmitCommandBuffer.hpp
        VulkanImageUtility.cpp
        GLTFMetallic_Roughness.cpp
        GLTFMetallic_Roughness.hpp
        SceneGraphMembers.hpp
        SceneGraphMembers.cpp
        SimpleCamera.cpp
        SimpleCamera.hpp
        Input.hpp
        Input.cpp
        Keys.hpp
        VulkanGeneralUtility.hpp
        VulkanGeneralUtility.cpp
        ShadowPipeline.cpp
        ShadowPipeline.hpp
)

# COMPILE SHADERS

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/shaders/*.frag"
        "${PROJECT_SOURCE_DIR}/shaders/*.vert"
        "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    message(STATUS COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(VulkanEngine Shaders)

# asset dependencies
# add_custom_target(copy_resources COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources)
# add_dependencies(VulkanEngine copy_resources)

# deps

# GLSL LANG
#if (VulkanHeaderVersion GREATER_EQUAL 275)
#    FetchContent_Declare(glslang
#            GIT_REPOSITORY https://github.com/KhronosGroup/glslang
#            GIT_TAG vulkan-sdk-1.3.${VulkanHeaderVersion}.0)
#    if (NOT glslang_POPULATED)
#        set(ENABLE_OPT OFF)
#    endif()
#    set(GLSLANG_LIB "glslang")
#else()
#    find_package(Vulkan REQUIRED SPIRV-Tools glslang)
#endif()
#FetchContent_MakeAvailable("${GLSLANG_LIB}")

message(vulkan-sdk-1.3.${VulkanHeaderVersion}.0)

include_directories(${Vulkan_INCLUDE_DIR})
message(STATUS "Vulkan_INCLUDE_DIR: ${Vulkan_INCLUDE_DIR}")
link_directories(${Vulkan_INCLUDE_DIR}/../lib)

#FetchContent_Declare(spirvreflect
#        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
#        GIT_TAG sdk-1.3.236.0)
#
#if(NOT spirvreflect_POPULATED)
#    set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "SPIRV_REFLECT_EXE" FORCE)
#    set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "SPIRV_REFLECT_EXAMPLES" FORCE)
#    set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "SPIRV_REFLECT_STATIC_LIB" FORCE)
#    FetchContent_Populate(spirvreflect)
#endif()
#add_subdirectory(${spirvreflect_SOURCE_DIR})
#FetchContent_MakeAvailable(spirvreflect)
#include_directories(${sprivreflect_SOURCE_DIR})

# FMT
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
)
FetchContent_MakeAvailable(fmt)
include_directories(${fmt_SOURCE_DIR}/include)
target_link_libraries(VulkanEngine PRIVATE fmt::fmt)

# VMA
FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.1.0
)
FetchContent_MakeAvailable(vma)
include_directories(${vma_SOURCE_DIR}/include)

# GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG 	bf71a834948186f4097caa076cd2663c69a10e1e #refs/tags/1.0.1
)
FetchContent_MakeAvailable(glm)
include_directories(${glm_SOURCE_DIR})
target_link_libraries(VulkanEngine PRIVATE glm::glm)

# VOLK
# https://github.com/zeux/volk - meta-loader for vulkan
# set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
# add_subdirectory(deps/volk)
# target_link_libraries(VulkanEngine PUBLIC volk)
FetchContent_Declare(
        volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG 1.3.270
)
FetchContent_MakeAvailable(volk)
include_directories(${volk_SOURCE_DIR})

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
)
if(NOT glfw_POPULATED)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Populate(glfw)
endif()

add_subdirectory(${glfw_SOURCE_DIR})
include_directories(${GLFW_SOURCE_DIR}/include)
target_link_libraries(VulkanEngine PRIVATE glfw)

# IMGUI
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.6
)
FetchContent_GetProperties(imgui)

if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
    target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
endif()
FetchContent_MakeAvailable(imgui)
include_directories(${imgui_SOURCE_DIR}/)
target_link_libraries(VulkanEngine PRIVATE imgui)

# STB
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
)
FetchContent_Populate(stb)
include_directories(${stb_SOURCE_DIR})

# TINYGLTF
FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
        GIT_TAG v2.9.2
)
if(NOT tinygltf_POPULATED)
    set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
    set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
endif()
FetchContent_MakeAvailable(tinygltf)
include_directories(${tinygltf_SOURCE_DIR})

# TRACY
#FetchContent_Declare(
#        tracy
#        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
#        GIT_TAG v0.9.1
#        GIT_SHALLOW TRUE
#        GIT_PROGRESS TRUE
#)
#
#message(WARNING "Tracy path: ${tracy_SOURCE_DIR}")
#FetchContent_MakeAvailable(tracy)
#include_directories(${tracy_SOURCE_DIR}/public)

#
#set(RequiredVulkanSDKLIBS
#        debug OSDependentd optimized OSDependent
#        # debug MachineIndependentd optimized MachineIndependent
#        # debug GenericCodeGend optimized GenericCodeGen
#        debug glslangd optimized glslang
#        debug SPIRVd optimized SPIRV
#        # debug SPIRV-Toolsd optimized SPIRV-Tools
#        # debug SPIRV-Tools-optd optimized SPIRV-Tools-opt
#        # debug glslang-default-resource-limitsd optimized glslang-default-resource-limits
#        # debug spirv-cross-cored optimized spirv-cross-core
#        # debug spirv-cross-glsld optimized spirv-cross-glsl
#        # debug spirv-cross-reflectd optimized spirv-cross-reflect)
#)

# link_libraries(${PROJECT_NAME} PUBLIC ${RequiredVulkanSDKLIBS})

# internal libraries

# BASE
#add_subdirectory(core)
#target_link_libraries(VulkanEngine PUBLIC core)
#target_include_directories(VulkanEngine PUBLIC core)
#
## GRAPHICS
#add_subdirectory(graphics)
#target_link_libraries(VulkanEngine PUBLIC graphics)
#target_include_directories(VulkanEngine PUBLIC graphics)
#
## APPLICATION
#add_subdirectory(application)
#target_link_libraries(VulkanEngine PUBLIC application)
#target_include_directories(VulkanEngine PUBLIC application)