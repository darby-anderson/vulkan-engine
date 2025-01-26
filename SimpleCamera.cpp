//
// Created by darby on 1/16/2025.
//

#include "SimpleCamera.hpp"

void SimpleCamera::init(Input* _input_module, glm::vec3 start_position) {
    this->input_module = _input_module;
    this->position = start_position;
}

void SimpleCamera::update() {

    // Reset velocity
    velocity[0] = 0; velocity[1] = 0; velocity[2] = 0;

    // Update our variables based on values in the input module

    if(input_module->is_key_down(KEY_W)) {
        velocity.z = -1;
    }

    if(input_module->is_key_down(KEY_S)) {
        velocity.z = 1;
    }

    if(input_module->is_key_down(KEY_A)) {
        velocity.x = -1;
    }

    if(input_module->is_key_down(KEY_D)) {
        velocity.x = 1;
    }

    if(input_module->is_key_down(KEY_LEFT_CONTROL)) {
        velocity *= 0.2f;
    }

    if(input_module->is_key_down(KEY_LEFT_SHIFT)) {
        velocity *= 5.f;
    }

    if(input_module->is_mouse_dragging(MOUSE_BUTTON_LEFT)) {
        CursorPosition clicked_pos = input_module->mouse_clicked_position[MOUSE_BUTTON_LEFT];
        CursorPosition current_pos = input_module->mouse_position;
        glm::vec2 mouse_movement = glm::vec2(current_pos.x, current_pos.y) - glm::vec2(clicked_pos.x, clicked_pos.y);

        drag_yaw = mouse_movement.x / 500;
        drag_pitch = mouse_movement.y / 500;

        is_dragging = true;

    } else if(is_dragging) { // was dragging last update, but not now
        yaw += drag_yaw;
        drag_yaw = 0;
        pitch += drag_pitch;
        drag_pitch = 0;

        is_dragging = false;
    }

    glm::mat4 cam_rot = get_rotation_matrix();
    position += glm::vec3(cam_rot * glm::vec4(velocity * 0.1f, 0.f));
}

glm::mat4 SimpleCamera::get_view_matrix() {
    glm::mat4 camera_translation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 camera_rot = get_rotation_matrix();
    return glm::inverse(camera_translation * camera_rot);
    // return camera_translation * camera_rot;
}

glm::mat4 SimpleCamera::get_rotation_matrix() {
    glm::quat pitch_rot = glm::angleAxis(pitch + drag_pitch, glm::vec3(1.f, 0.f, 0.f));
    glm::quat yaw_rot = glm::angleAxis(yaw + drag_yaw, glm::vec3(0.f, -1.f, 0.f));

    return glm::toMat4(yaw_rot) * glm::toMat4(pitch_rot);
}


