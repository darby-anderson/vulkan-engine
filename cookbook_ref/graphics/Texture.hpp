//
// Created by darby on 6/4/2024.
//

#pragma once

#include <unordered_map>
#include "volk.h"
#include "vk_mem_alloc.h"
#include "Context.hpp"
#include "Buffer.hpp"

namespace Fairy::Graphics {

class Context;

VkResult checkImageFormatProperties(Context& context, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkImageCreateFlags createFlags);

class Texture {

public:
    explicit Texture(const Context& context, VkImageType type, VkFormat format, VkImageCreateFlags createFlags, VkImageUsageFlags usage,
                     VkExtent3D extent, uint32_t mipLevels, uint32_t layerCount, VkMemoryPropertyFlags memoryFlags,
                     bool generateMips = false, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT,
                     const std::string& name = "", bool multiview = false,
                     VkImageTiling imageTiling = VK_IMAGE_TILING_OPTIMAL);

    // Constructor for images created by swapchain:
    explicit Texture(const Context& context, VkDevice device, VkImage image, VkFormat format, VkExtent3D extents,
                     uint32_t numLayers = 1, bool multiView = false, const std::string& name = "");

    ~Texture();

    VkImage image() const;
    VkImageView view(uint32_t mipLevel = UINT32_MAX);
    VkFormat format() const;
    VkImageLayout layout() const;
    VkExtent3D extent() const ;
    VkDeviceSize deviceSize() const;

    [[nodiscard]] bool isDepth() const;
    [[nodiscard]] bool isStencil() const;

    void uploadAndGenerateMips(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer, void* data);

    void uploadTexture(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer, void* data, uint32_t layer = 0);

    void generateMipmaps(VkCommandBuffer cmdBuffer);

    void transitionImageLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout);

    uint32_t pixelSizeInBytes() const;

private:

    uint32_t getMipLevelsCount(uint32_t texWidth, uint32_t texHeight) const;

    VkImageView createImageView(const Context& context, VkImageViewType viewType, VkFormat format, uint32_t mipLevels,
                                uint32_t layerCount, const std::string& name);

    const Context& context_;
    VmaAllocator vmaAllocator_ = nullptr;

    VkImage image_ = VK_NULL_HANDLE;
    VmaAllocation vmaAllocation_ = nullptr;
    VkImageView view_ = VK_NULL_HANDLE;
    VkImageUsageFlags usageFlags_ = 0;
    VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent3D extent_;
    VkDeviceSize deviceSize_ = 0;
    VkImageCreateFlags createFlags_;

    VkFormat format_;
    VkImageType imageType_;

    bool ownsVkImage_;
    bool generateMips_;

    VkImageTiling imageTiling_;
    uint32_t mipLevels_;
    bool multiView_;
    uint32_t layerCount_;
    std::string debugName_;
    VkSampleCountFlagBits msaaSamples_;
    VkImageViewType viewType_;

    std::unordered_map<uint32_t, VkImageView> imageViewFramebuffers_;

    VkImageLayout imageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;

};

}
