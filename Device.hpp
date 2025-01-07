//
// Created by darby on 11/24/2024.
//

#pragma once

#include "Common.hpp"

class Device {

public:
    VkDevice device;

    std::optional<uint32_t> family_index_graphics;
    std::optional<uint32_t> family_index_presentation;

    VkQueue graphics_queue;
    VkQueue presentation_queue;

    void init(VkPhysicalDevice physical_device, VkSurfaceKHR surface);
    void cleanup();



private:
    VkPhysicalDevice physical_device;

    void reserve_queues(VkSurfaceKHR surface);

};

