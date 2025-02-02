//
// Created by darby on 1/16/2025.
//

#pragma once

#include "Common.hpp"
#include "Input.hpp"

class SimpleCamera {

public:

    void init(Input* input_module, glm::vec3 start_position = glm::vec3{0});
    void update();
    glm::mat4 get_view_matrix();
    glm::mat4 get_rotation_matrix();
    glm::vec3 get_position();

private:
    glm::vec3 position {0};
    glm::vec3 velocity {0};
    float yaw = 0;
    float pitch = 0;

    bool is_dragging = false;
    float drag_yaw = 0;
    float drag_pitch = 0;

    Input* input_module;

};

