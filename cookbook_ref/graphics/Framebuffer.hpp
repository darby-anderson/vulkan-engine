//
// Created by darby on 6/4/2024.
//

#pragma once

#include "volk.h"
#include "Texture.hpp"
#include <vector>
#include <memory>

namespace Fairy::Graphics {

// Framebuffer defines the final render target
class Framebuffer {

public:
    Framebuffer(const Context& context, VkDevice device, VkRenderPass renderPass,
                std::vector<std::shared_ptr<Texture>> attachments, std::shared_ptr<Texture> depthAttachment,
                std::shared_ptr<Texture> stencilAttachment, const std::string& name);

    ~Framebuffer();

    VkFramebuffer framebuffer() { return framebuffer_; }

private:
    VkDevice device_;
    VkRenderPass renderPass_;

    VkFramebuffer framebuffer_;

};

}