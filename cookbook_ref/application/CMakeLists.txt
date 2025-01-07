add_library(
        application
        "Window.hpp"
        "Window.cpp"
        RingBuffer.cpp
        RingBuffer.hpp
        Camera.cpp
        Camera.hpp
        GLTFLoader.cpp
        GLTFLoader.hpp
        ImguiManager.cpp
        ImguiManager.hpp
        Input.cpp
        Input.hpp
        Keys.hpp
)

target_link_libraries(
        application
        PRIVATE
        imgui
        glfw
        graphics
)

target_include_directories(application PUBLIC ${stb_SOURCE_DIR})
target_include_directories(application PUBLIC ${tinygltf_SOURCE_DIR})
target_include_directories(application PUBLIC ${imgui_SOURCE_DIR}/)

target_include_directories(application PUBLIC ../graphics)