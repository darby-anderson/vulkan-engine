add_library(
        graphics
        "VulkanUtils.hpp"
        "VulkanUtils.cpp"
        "Context.cpp"
        "Context.hpp"
        PhysicalDevice.cpp
        PhysicalDevice.hpp
        CommandQueueManager.cpp
        CommandQueueManager.hpp
        Texture.cpp
        Texture.hpp
        RenderPass.cpp
        RenderPass.hpp
        Framebuffer.cpp
        Framebuffer.hpp
        ShaderModule.cpp
        ShaderModule.hpp
        Pipeline.cpp
        Pipeline.hpp
        Swapchain.cpp
        Swapchain.hpp
        vma_initializer.cpp
        Buffer.cpp
        Buffer.hpp
        Sampler.cpp
        Sampler.hpp
        Model.hpp
        Model.cpp
        Model.cpp
        DynamicRendering.cpp
        DynamicRendering.hpp
)

target_link_libraries(
        graphics
        PRIVATE
        core
        volk
        # tracy
        TracyClient
        glslang
        glslang-default-resource-limits
        OSDependent
        SPIRV
        spirv-reflect-static
)

target_include_directories(graphics PUBLIC ../deps/stb)

include_directories(${Vulkan_INCLUDE_DIR})
message(STATUS "Vulkan_INCLUDE_DIR: ${Vulkan_INCLUDE_DIR}")
target_link_directories(graphics PUBLIC ${Vulkan_INCLUDE_DIR}/../lib)

target_include_directories(graphics PUBLIC ../core)
message("VMA including this:" + ${vma_SOURCE_DIR}/include)
target_include_directories(graphics PUBLIC ${vma_SOURCE_DIR}/include)
target_include_directories(graphics PUBLIC ${spirvreflect_SOURCE_DIR})
message("spirvreflect including this:" + ${spirvreflect_SOURCE_DIR})
