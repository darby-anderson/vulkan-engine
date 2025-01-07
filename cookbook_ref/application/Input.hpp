//
// Created by darby on 11/8/2024.
//

#pragma once

#include <cstdint>
#include "Window.hpp"
#include "Keys.hpp"

namespace Fairy::Application {

// These match GLFW's mouse enum
enum MouseButton {
    MOUSE_BUTTON_LEFT = 0,
    MOUSE_BUTTON_RIGHT = 1,
    MOUSE_BUTTON_MIDDLE = 2,
    MOUSE_BUTTON_LAST = 7,
    MOUSE_BUTTON_COUNT = 8
};

class Input {

public:

    void init(Window* config);
    void start_new_frame();

    bool is_key_down(Key key);
    bool is_key_just_pressed(Key key);
    bool is_key_just_released(Key key);

    bool is_mouse_button_down(MouseButton button);
    bool is_mouse_button_just_pressed(MouseButton button);
    bool is_mouse_button_just_released(MouseButton button);

    bool is_mouse_dragging(MouseButton button);

    uint8_t keys[KEY_COUNT];
    uint8_t first_frame_keys[KEY_COUNT];
    uint8_t released_keys[KEY_COUNT];

    uint8_t mouse_buttons[MOUSE_BUTTON_COUNT];
    uint8_t first_frame_mouse_buttons[MOUSE_BUTTON_COUNT];
    uint8_t released_mouse_buttons[MOUSE_BUTTON_COUNT];

    CursorPosition mouse_position = { 0.0f, 0.0f };
    CursorPosition previous_mouse_position = { 0.0f, 0.0f };
    CursorPosition mouse_clicked_position[MOUSE_BUTTON_COUNT];
    float          mouse_drag_distance[MOUSE_BUTTON_COUNT];

    Window* window;
};

};
