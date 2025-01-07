//
// Created by darby on 12/7/2024.
//

#pragma once

#include "volk.h"

namespace vk_image {

    void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D source_size, VkExtent2D destination_size);

}
