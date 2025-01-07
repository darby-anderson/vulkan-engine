//
// Created by darby on 6/4/2024.
//

#pragma once

#include <memory>
#include "Context.hpp"
#include "Texture.hpp"

namespace Fairy::Graphics {

class Context;

class Texture;

class RenderPass final {

public:
    explicit RenderPass() = default;

    explicit RenderPass(const Context& context,
                        const std::vector<std::shared_ptr<Texture>> attachments,
                        const std::vector<VkAttachmentLoadOp>& loadOp,
                        const std::vector<VkAttachmentStoreOp>& storeOp,
                        const std::vector<VkImageLayout>& layout,
                        VkPipelineBindPoint bindPoint,
                        const std::string& name);

    ~RenderPass();

    VkRenderPass renderPass() const { return renderPass_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_;

};

}