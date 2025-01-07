//
// Created by darby on 11/20/2024.
//

#pragma once

#include <cstdint>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>


struct WindowConfiguration {
    uint32_t             width;
    uint32_t             height;
};

struct CursorPosition {
    double x;
    double y;
};

struct Window {
    void            init(const WindowConfiguration& configuration);
    void            shutdown();

    void            request_os_messages();

    void            set_fullscreen(bool value);

//        void            register_os_messages_callback(OsMessagesCallback callback, void* user_data);
//        void            deregister_os_messages_callback(OsMessagesCallback callback);


    void            center_mouse(bool dragging);

    bool            has_focus();

    bool            should_exit();

    void            terminate();

    CursorPosition  get_window_cursor_position();

    GLFWwindow*     get_glfw_window();
    HWND            get_win32_window();

    void            set_window_user_pointer(void* user);
    void            set_key_press_callback(GLFWkeyfun callback);
    void            set_mouse_button_press_callback(GLFWmousebuttonfun callback);
    void            set_framebuffer_resize_callback(GLFWframebuffersizefun callback);

    void            get_window_framebuffer_size(int& width, int& height) const;

    void            get_primary_monitor_resolution(int& width, int& height) const;

    void*           platform_handle     = nullptr;


    // bool            request_exist       = false;
    bool            resized             = false;
    bool            minimized           = false;
    uint32_t        width               = 0;
    uint32_t        height              = 0;
    float           display_refresh     = 1.0f / 60.0f;

};