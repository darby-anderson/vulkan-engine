//
// Created by darby on 6/4/2024.
//

#include "CoreUtils.hpp"
#include "RenderPass.hpp"
#include "VulkanUtils.hpp"

namespace Fairy::Graphics {

RenderPass::RenderPass(const Context& context, const std::vector<std::shared_ptr<Texture>> attachments,
                       const std::vector<VkAttachmentLoadOp>& loadOp, const std::vector<VkAttachmentStoreOp>& storeOp,
                       const std::vector<VkImageLayout>& layouts, VkPipelineBindPoint bindPoint,
                       const std::string& name) : device_(context.device()) {

    ASSERT(attachments.size() == loadOp.size() &&
           attachments.size() == storeOp.size() &&
           attachments.size() == layouts.size(),
           "The sizes of attachments and their load and store ops and final layouts must match");

    std::vector<VkAttachmentDescription> attachmentDescs; // properties of an attachment, attachments being images targeted during rendering. can be color or depth/stencil
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    std::optional<VkAttachmentReference> depthStencilAttachmentRef;

    for (uint32_t i = 0; i < attachments.size(); i++) {
        attachmentDescs.emplace_back(
            VkAttachmentDescription{
                .format = attachments[i]->format(),
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = attachments[i]->isStencil() ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : loadOp[i],
                .storeOp = attachments[i]->isStencil() ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOp[i],
                .stencilLoadOp = attachments[i]->isStencil() ? loadOp[i] : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = attachments[i]->isStencil() ? storeOp[i] : VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = attachments[i]->layout(),
                .finalLayout = layouts[i]
            }
        );

        if (attachments[i]->isStencil() || attachments[i]->isDepth()) {
            depthStencilAttachmentRef = VkAttachmentReference{
                .attachment = i,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            };
        } else {
            colorAttachmentRefs.emplace_back(
                VkAttachmentReference{
                    .attachment = i,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                }
            );
        }
    }

    const VkSubpassDescription spd = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size()),
        .pColorAttachments = colorAttachmentRefs.data(),
        .pDepthStencilAttachment = depthStencilAttachmentRef.has_value() ? &depthStencilAttachmentRef.value() : nullptr
    };

    const VkSubpassDependency subpassDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    const VkRenderPassCreateInfo rpci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(attachmentDescs.size()),
        .pAttachments = attachmentDescs.data(),
        .subpassCount = 1,
        .pSubpasses = &spd,
        .dependencyCount = 1,
        .pDependencies = &subpassDependency
    };

    VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_));

}

RenderPass::~RenderPass() {
    vkDestroyRenderPass(device_, renderPass_, nullptr);
}

}