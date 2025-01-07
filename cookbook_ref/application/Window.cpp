//
// Created by darby on 5/29/2024.
//

#include <iostream>
#include "window.hpp"

#include "GLFW/glfw3.h"

static GLFWwindow* window = nullptr;
static GLFWmonitor* monitor = nullptr;

const char* get_glfw_error() {
    const char* err = nullptr;
    glfwGetError(&err);
    return err;
}

namespace Fairy::Application {

void Window::init(const WindowConfiguration& configuration) {
    std::cout << "Window Service" << std::endl;

    if (glfwInit() == GLFW_FALSE) {
        std::cout << "GLFW Error \n" << *get_glfw_error() << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(configuration.width, configuration.height, "Puffin::Vulkan", nullptr, nullptr);

#if defined(WIN32)
    HWND hwnd = glfwGetWin32Window(window);
    SetWindowLongPtr(
        hwnd, GWL_STYLE,
        GetWindowLongPtrA(hwnd, GWL_STYLE) & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX));
#endif

    // Store data for use with GLFW's callbacks
    glfwSetWindowUserPointer(window, reinterpret_cast<void*>(this));

    int window_width, window_height;
    glfwGetWindowSize(window, &window_width, &window_height);
    width = (uint32_t) window_width;
    height = (uint32_t) window_height;

    // Assigning this for outside use
    platform_handle = window;

    // OS Messages
//        os_messages_callbacks.init(config.allocator, 4);
//        os_messages_callbacks_data.init(config.allocator, 4);

//        glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
//        glfwSetKeyCallback(window, key_pressed_callback);
//        glfwSetMouseButtonCallback(window, mouse_button_pressed_callback);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (mode != nullptr) {
        display_refresh = mode->refreshRate;
    }
}


bool Window::has_focus() {
    return glfwGetWindowAttrib(window, GLFW_FOCUSED);
}

void Window::shutdown() {
//        os_messages_callbacks_data.shutdown();
//        os_messages_callbacks.shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void Window::set_fullscreen(bool value) {
    if (value) {
        // TODO SET GLFW FULLSCREEN
    } else {
        // TODO SET GLFW NOT FULLSCREEN
    }
}

void Window::request_os_messages() {
    glfwPollEvents();
}

bool Window::should_exit() {
    return glfwWindowShouldClose(window);
}


void Window::center_mouse(bool dragging) {
    // TODO - CENTER MOUSE WITH GLFW
}

void Window::set_window_user_pointer(void* user) {
    glfwSetWindowUserPointer(window, user);
}

void Window::set_key_press_callback(GLFWkeyfun callback) {
    glfwSetKeyCallback(window, callback);
}

void Window::set_mouse_button_press_callback(GLFWmousebuttonfun callback) {
    glfwSetMouseButtonCallback(window, callback);
}

void Window::set_framebuffer_resize_callback(GLFWframebuffersizefun callback) {
    glfwSetFramebufferSizeCallback(window, callback);
}

CursorPosition Window::get_window_cursor_position() {
    CursorPosition cursor_position{};
    glfwGetCursorPos(window, &cursor_position.x, &cursor_position.y);
    return cursor_position;
}

HWND Window::get_win32_window() {
    return glfwGetWin32Window(window);
}

}