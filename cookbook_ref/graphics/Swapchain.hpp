//
// Created by darby on 6/5/2024.
//

#pragma once

#include <memory>
#include "volk.h"
#include "Context.hpp"
#include "Texture.hpp"
#include "RenderPass.hpp"

namespace Fairy::Graphics {

class Context;

class Framebuffer;

class PhysicalDevice;

class Texture;

// A Swapchain is essentially a queue of images that are waiting to be presented to the screen
// The driver OWNs these images

class Swapchain final {

public:
    explicit Swapchain() = default;

    explicit Swapchain(const Context& context, const PhysicalDevice& physicalDevice, VkSurfaceKHR surface,
                       VkQueue presentQueue,
                       VkFormat imageFormat, VkColorSpaceKHR imageColorSpace, VkPresentModeKHR presentMode,
                       VkExtent2D extent,
                       const std::string& name = "");

    ~Swapchain();

    std::shared_ptr<Texture> acquireImage();

    void present() const;

    VkSubmitInfo createSubmitInfo(const VkCommandBuffer* buffer,
                                  const VkPipelineStageFlags* submitStageMask,
                                  bool waitForImageAvailable = true,
                                  bool signalImagePresented = true) const;

    VkSwapchainKHR swapchain();

    uint32_t imageCount() const { return textures_.size(); }

    uint32_t currentImageIndex() const { return imageIndex_; };

    std::shared_ptr<Texture> textureAtIndex(uint32_t index);

    VkExtent2D extent() const { return extent_; }

private:

    void createTextures(const Context& context, VkFormat format, VkExtent2D extent);

    void createSemaphores(const Context& context);

    std::vector<std::shared_ptr<Texture>> textures_;

    VkFence acquireFence_ = VK_NULL_HANDLE;
    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore imageRendered_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkExtent2D extent_;

    uint32_t imageIndex_ = 0;

};

}