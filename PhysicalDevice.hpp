//
// Created by darby on 11/25/2024.
//

#pragma once

#include "Common.hpp"


class PhysicalDevice {
public:
    void choose_and_init(VkInstance instance, VkSurfaceKHR surface);

    VkPhysicalDevice physical_device;

    VkSurfaceCapabilitiesKHR surface_capabilities;
    std::vector<VkSurfaceFormatKHR> surface_formats;
    std::vector<VkPresentModeKHR> present_modes;

};

