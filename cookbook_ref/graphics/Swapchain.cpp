//
// Created by darby on 6/5/2024.
//

#include <array>
#include <memory>
#include <algorithm>
#include "Swapchain.hpp"
#include "Context.hpp"
#include "VulkanUtils.hpp"

namespace Fairy::Graphics {

Swapchain::Swapchain(const Context& context, const PhysicalDevice& physicalDevice, VkSurfaceKHR surface,
                     VkQueue presentQueue,
                     VkFormat imageFormat, VkColorSpaceKHR imageColorSpace, VkPresentModeKHR presentMode,
                     VkExtent2D extent,
                     const std::string& name) : device_(context.device()), presentQueue_(presentQueue),
                                                extent_(extent) {
    const uint32_t numImages =
        std::clamp(/*physicalDevice.surfaceCapabilities().minImageCount + 1*/ 3u,
                   physicalDevice.surfaceCapabilities().minImageCount,
                   physicalDevice.surfaceCapabilities().maxImageCount);

    const bool presentationQueueIsShared =
        physicalDevice.getGraphicsFamilyIndex().value() == physicalDevice.getPresentationFamilyIndex().value();

    std::array<uint32_t, 2> familyIndices = {
        physicalDevice.getGraphicsFamilyIndex().value(),
        physicalDevice.getPresentationFamilyIndex().value()
    };

    const VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = numImages,
        .imageFormat = imageFormat,
        .imageColorSpace = imageColorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = presentationQueueIsShared ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = presentationQueueIsShared ? 0u : 2u,
        .pQueueFamilyIndices = presentationQueueIsShared ? nullptr : familyIndices.data(),
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    VK_CHECK(vkCreateSwapchainKHR(device_, &swapchainCreateInfo, nullptr, &swapchain_));
    context.setVkObjectName(swapchain_, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "Swapchain: " + name);

    createTextures(context, imageFormat, extent);
    createSemaphores(context);

    const VkFenceCreateInfo fenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr, &acquireFence_));
    context.setVkObjectName(acquireFence_, VK_OBJECT_TYPE_FENCE, "Swapchain Acquire Fence");
}

Swapchain::~Swapchain() {
    VK_CHECK(vkWaitForFences(device_, 1, &acquireFence_, VK_TRUE, UINT64_MAX));
    vkDestroyFence(device_, acquireFence_, nullptr);
    vkDestroySemaphore(device_, imageRendered_, nullptr);
    vkDestroySemaphore(device_, imageAvailable_, nullptr);
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
}

std::shared_ptr<Texture> Swapchain::acquireImage() {

    VK_CHECK(vkWaitForFences(device_, 1, &acquireFence_, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device_, 1, &acquireFence_));

    vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailable_, acquireFence_, &imageIndex_);

    return textures_[imageIndex_];
}

void Swapchain::present() const {
    const VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageRendered_,
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &imageIndex_
    };
    VK_CHECK(vkQueuePresentKHR(presentQueue_, &presentInfo));
}

VkSubmitInfo Swapchain::createSubmitInfo(const VkCommandBuffer* buffer, const VkPipelineStageFlags* submitStageMask,
                                         bool waitForImageAvailable, bool signalImagePresented) const {

    const VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = waitForImageAvailable ? (imageAvailable_ ? 1u : 0)
                                                    : 0, // signals once swapchain image is ready to be accessed and rendered to
        .pWaitSemaphores = waitForImageAvailable ? &imageAvailable_ : VK_NULL_HANDLE,
        .pWaitDstStageMask = submitStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = buffer,
        .signalSemaphoreCount = signalImagePresented ? (imageRendered_ ? 1u : 0)
                                                     : 0, // signals once the command buffer is executed and ready to write to a swapchain image
        .pSignalSemaphores = signalImagePresented ? &imageRendered_ : VK_NULL_HANDLE,
    };

    return si;
}


void Swapchain::createTextures(const Context& context, VkFormat format, VkExtent2D extent) {

    uint32_t imageCount{0};
    VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr));
    std::vector<VkImage> vkImages(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, vkImages.data()));

    textures_.reserve(imageCount);
    for (size_t index = 0; index < imageCount; index++) {
        textures_.emplace_back(
            std::make_shared<Texture>(
                context,
                device_,
                vkImages[index],
                format,
                VkExtent3D{
                    .width = extent.width,
                    .height = extent.height,
                    .depth = 1
                },
                1,
                false,
                "Swapchain Image " + std::to_string(index)));
    }
}

void Swapchain::createSemaphores(const Context& context) {
    const VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &imageAvailable_));
    context.setVkObjectName(imageAvailable_, VK_OBJECT_TYPE_SEMAPHORE,
                            "Semaphore: swapchain image available semaphore");

    VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &imageRendered_));
    context.setVkObjectName(imageRendered_, VK_OBJECT_TYPE_SEMAPHORE,
                            "Semaphore: swapchain image presented semaphore");
}

VkSwapchainKHR Swapchain::swapchain() {
    return swapchain_;
}

std::shared_ptr<Texture> Swapchain::textureAtIndex(uint32_t index) {
    ASSERT(index < textures_.size(), "Getting swapchain texture index out of bounds. Index: " + std::to_string(index));
    return textures_[index];
}

}