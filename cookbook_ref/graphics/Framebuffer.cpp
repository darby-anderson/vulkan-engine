//
// Created by darby on 6/4/2024.
//

#include "Framebuffer.hpp"
#include "VulkanUtils.hpp"

namespace Fairy::Graphics {

Framebuffer::Framebuffer(const Context& context, VkDevice device, VkRenderPass renderPass,
                         const std::vector<std::shared_ptr<Texture>> attachments,
                         const std::shared_ptr<Texture> depthAttachment,
                         const std::shared_ptr<Texture> stencilAttachment, const std::string& name)
    : device_(device), renderPass_(renderPass) {

    // get all image views
    std::vector<VkImageView> imageViews;
    for (int i = 0; i < attachments.size(); i++) {
        imageViews.emplace_back(attachments[i]->view());
    }

    if (depthAttachment) {
        imageViews.emplace_back(depthAttachment->view());
    }

    if (stencilAttachment) {
        imageViews.emplace_back(stencilAttachment->view());
    }

    ASSERT(!imageViews.empty(), "Creating a framebuffer with no attachments is not supported");

    const uint32_t width = !attachments.empty() ? attachments[0]->extent().width
                                                : depthAttachment->extent().width;

    const uint32_t height = !attachments.empty() ? attachments[0]->extent().height
                                                 : depthAttachment->extent().height;

    const VkFramebufferCreateInfo fci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass_,
        .attachmentCount = static_cast<uint32_t>(imageViews.size()),
        .pAttachments = imageViews.data(),
        .width = width,
        .height = height,
        .layers = 1,
    };

    VK_CHECK(vkCreateFramebuffer(device_, &fci, nullptr, &framebuffer_));

}

Framebuffer::~Framebuffer() { vkDestroyFramebuffer(device_, framebuffer_, nullptr); }

}