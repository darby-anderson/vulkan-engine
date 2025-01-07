//
// Created by darby on 12/17/2024.
//

#pragma once

#include "Common.hpp"
#include "ComputePipeline.hpp"

struct ComputePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect {
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};
