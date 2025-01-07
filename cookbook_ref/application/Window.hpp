//
// Created by darby on 5/29/2024.
//
#pragma once

#include <cstdint>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

namespace Fairy::Application {

struct WindowConfiguration {
    uint32_t             width;
    uint32_t             height;
};

struct CursorPosition {
    double x;
    double y;
};

typedef void        (*OsMessagesCallback) (void* os_event, void* user_data);

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

    CursorPosition  get_window_cursor_position();

    HWND            get_win32_window();

    void            set_window_user_pointer(void* user);
    void            set_key_press_callback(GLFWkeyfun callback);
    void            set_mouse_button_press_callback(GLFWmousebuttonfun callback);
    void            set_framebuffer_resize_callback(GLFWframebuffersizefun callback);

    void*           platform_handle     = nullptr;


    // bool            request_exist       = false;
    bool            resized             = false;
    bool            minimized           = false;
    uint32_t        width               = 0;
    uint32_t        height              = 0;
    float           display_refresh     = 1.0f / 60.0f;

};

}
