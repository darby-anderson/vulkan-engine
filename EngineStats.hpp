//
// Created by darby on 2/9/2025.
//

#pragma once


struct EngineStats {
    float frame_time;
    float longest_frame_time = 0.0f;
    int longest_frame_number = -1;
    int triangle_count;
    int draw_call_count;
    float scene_update_time;
    float mesh_draw_time;
    float lighting_draw_time;
};
