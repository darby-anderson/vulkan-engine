//
// Created by darby on 6/4/2024.
//

#include <cmath>
#include "Texture.hpp"
#include "Context.hpp"
#include "VulkanUtils.hpp"

#include "vk_mem_alloc.h"

namespace Fairy::Graphics {

VkResult checkImageFormatProperties(Context& context, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkImageCreateFlags createFlags) {

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .format = format,
        .type = type,
        .tiling = tiling,
        .usage = usageFlags,
    };

    VkImageFormatProperties2 formatInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2
    };

    VkResult result = vkGetPhysicalDeviceImageFormatProperties2(context.physicalDevice().getPhysicalDevice(), &imageFormatInfo, &formatInfo);

    std::cout << std::endl;
    std::cout << "Physical Device Image Format Properties Info" << std::endl;
    std::cout << "Max extent | (width: " << formatInfo.imageFormatProperties.maxExtent.width << ", height: " << formatInfo.imageFormatProperties.maxExtent.height << ", depth: " << formatInfo.imageFormatProperties.maxExtent.depth << ")" << std::endl;
    std::cout << "Max mip levels | " << formatInfo.imageFormatProperties.maxMipLevels << std::endl;
    std::cout << "Max array layers | " << formatInfo.imageFormatProperties.maxArrayLayers << std::endl;
    // more data can be retrieved

    return result;
}


Texture::Texture(const Context& context, VkImageType imageType, VkFormat format, VkImageCreateFlags createFlags, VkImageUsageFlags usage,
                 VkExtent3D extent, uint32_t mipLevels, uint32_t layerCount, VkMemoryPropertyFlags memoryFlags,
                 bool generateMips, VkSampleCountFlagBits msaaSamples, const std::string& name,
                 bool multiview, VkImageTiling imageTiling)
    : context_(context),
    vmaAllocator_(context.allocator()),
    createFlags_(createFlags),
    imageType_(imageType),
    format_(format),
    extent_(extent),
    ownsVkImage_(true),
    mipLevels_(mipLevels),
    layerCount_(layerCount),
    multiView_(multiview),
    generateMips_(generateMips),
    msaaSamples_(msaaSamples),
    imageTiling_(imageTiling),
    usageFlags_(usage),
    debugName_(name)
    {

    ASSERT(extent_.width > 0 && extent_.height > 0, "Texture cannot have dimensions equal to 0");
    ASSERT(mipLevels_ > 0, "Texture needs at least 1 mip level");

    if(generateMips_) {
        mipLevels_ = getMipLevelsCount(extent_.width, extent_.height);
    }

    ASSERT(!(mipLevels_ > 1 && msaaSamples_ != VK_SAMPLE_COUNT_1_BIT),
           "Multisampled images cannot have more than 1 mip level");

    const VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = createFlags_,
        .imageType = imageType_,
        .format = format_,
        .extent = extent_,
        .mipLevels = mipLevels_,
        .arrayLayers = layerCount_,
        .samples = msaaSamples_,
        .tiling = imageTiling_,
        .usage = usageFlags_,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
            : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .priority = 1.0f,
    };

    VK_CHECK(vmaCreateImage(vmaAllocator_, &imageCreateInfo, &allocCreateInfo, &image_, &vmaAllocation_, nullptr));

    if(vmaAllocation_ != nullptr) {
        VmaAllocationInfo allocationInfo;
        vmaGetAllocationInfo(vmaAllocator_, vmaAllocation_, &allocationInfo);
        deviceSize_ = allocationInfo.size;
    }

    context.setVkObjectName(image_, VK_OBJECT_TYPE_IMAGE, "Image: " + name);

    const VkImageViewType imageViewType = imageTypeToImageViewType(imageType_, createFlags_, multiView_);
    viewType_ = imageViewType;

    view_ = createImageView(context_, viewType_, format_, mipLevels_, layerCount_, debugName_);
}


Texture::Texture(const Context& context, VkDevice device, VkImage image, VkFormat format, VkExtent3D extent,
                 uint32_t numLayers, bool multiView, const std::string& name)
               : context_(context),
                 image_(image),
                 format_(format),
                 extent_(extent),
                 layerCount_(numLayers),
                 multiView_(multiView),
                 ownsVkImage_(false),
                 debugName_(name) {

    context.setVkObjectName(image_, VK_OBJECT_TYPE_IMAGE, "Vulkan-Owned Image " + name);

    view_ = createImageView(context_, multiView_ ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
                            format_, 1, layerCount_, debugName_);
}

Texture::~Texture() {
    for(const auto imageView : imageViewFramebuffers_) {
        vkDestroyImageView(context_.device(), imageView.second, nullptr);
    }

    vkDestroyImageView(context_.device(), view_, nullptr);

    if(ownsVkImage_) {
        vmaDestroyImage(vmaAllocator_, image_, vmaAllocation_);
    }
}

VkImageView Texture::view(uint32_t mipLevel) {
    ASSERT(mipLevel == UINT32_MAX || mipLevel < mipLevels_, "Invalid mip level");

    if(mipLevel == UINT32_MAX) {
        return view_;
    }

    if(!imageViewFramebuffers_.contains(mipLevel)) {
        const VkImageViewType  imageViewType = imageTypeToImageViewType(imageType_, createFlags_, multiView_);

        imageViewFramebuffers_[mipLevel] = createImageView(context_, imageViewType, format_, 1, layerCount_,
                                                           "Image View for Framebuffer: " + debugName_);
    }

    return imageViewFramebuffers_[mipLevel];
}

VkImage Texture::image() const {
    return image_;
}

VkFormat Texture::format() const {
    return format_;
}

bool Texture::isDepth() const {
    return (
        format_ == VK_FORMAT_D16_UNORM ||
        format_ == VK_FORMAT_D16_UNORM_S8_UINT ||
        format_ == VK_FORMAT_D24_UNORM_S8_UINT ||
        format_ == VK_FORMAT_D32_SFLOAT ||
        format_ == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        format_ == VK_FORMAT_X8_D24_UNORM_PACK32
    );
}

bool Texture::isStencil() const {
    return (
        format_ == VK_FORMAT_S8_UINT ||
        format_ == VK_FORMAT_D16_UNORM_S8_UINT ||
        format_ == VK_FORMAT_D24_UNORM_S8_UINT ||
        format_ == VK_FORMAT_D32_SFLOAT_S8_UINT
    );
}

VkImageLayout Texture::layout() const {
    return layout_;
}

VkExtent3D Texture::extent() const {
    return extent_;
}

VkDeviceSize Texture::deviceSize() const {
    return deviceSize_;
}

VkImageView
Texture::createImageView(const Context& context, VkImageViewType viewType, VkFormat format, uint32_t mipLevels,
                         uint32_t layerCount, const std::string& name) {

    const VkImageViewCreateInfo imageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image_,
        .viewType = viewType,
        .format = format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask =  isDepth() ? VK_IMAGE_ASPECT_DEPTH_BIT : (isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT
                                                                                : VK_IMAGE_ASPECT_COLOR_BIT),
            .baseMipLevel = 0,
            .levelCount = mipLevels,
            .baseArrayLayer = 0,
            .layerCount = layerCount
        }
    };

    VkImageView imageView;
    VK_CHECK(vkCreateImageView(context.device(), &imageViewCreateInfo, nullptr, &imageView));
    context.setVkObjectName(imageView, VK_OBJECT_TYPE_IMAGE_VIEW, "Image view: " + name);

    return imageView;
}

uint32_t Texture::getMipLevelsCount(uint32_t texWidth, uint32_t texHeight) const {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
}

void Texture::uploadAndGenerateMips(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer, void* data) {

    uploadTexture(cmdBuffer, stagingBuffer, data);

    context_.beginDebugUtilsLabel(cmdBuffer, "Transition to Shader_Read_Only & Generate mips", {1.0f, 0.0f, 0.0f, 1.0f});

    generateMipmaps(cmdBuffer);
    if(layout_ != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        transitionImageLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    context_.endDebugUtilsLabel(cmdBuffer);
}

void Texture::uploadTexture(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer, void* data, uint32_t layer) {

    context_.beginDebugUtilsLabel(cmdBuffer, "Uploading image", {
        1.0f, 0.0f, 0.0f, 1.0f
    });

    stagingBuffer->copyDataToBuffer(
        data, pixelSizeInBytes() * extent_.width * extent_.height * extent_.depth
        );

    if(layout_ == VK_IMAGE_LAYOUT_UNDEFINED) {
        transitionImageLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    const VkImageAspectFlags aspectMask = isDepth() ?
                                          isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT
                                                      : VK_IMAGE_ASPECT_DEPTH_BIT
                                                    : VK_IMAGE_ASPECT_COLOR_BIT;

    const VkBufferImageCopy bufCopy = {
        .bufferOffset = 0,
        .imageSubresource = {
            .aspectMask = aspectMask,
            .mipLevel = 0,
            .baseArrayLayer = layer,
            .layerCount = 1
        },
        .imageOffset = {
            .x = 0,
            .y = 0,
            .z = 0
        },
        .imageExtent = extent_
    };

    vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer->vkBuffer(), image_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufCopy);
    context_.endDebugUtilsLabel(cmdBuffer);
}

void Texture::generateMipmaps(VkCommandBuffer cmdBuffer) {

    if(!generateMips_) {
        return;
    }

    context_.beginDebugUtilsLabel(cmdBuffer, "Generate Mips", {
        0.0f, 1.0f, 0.0f, 1.0f
    });

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(context_.physicalDevice().getPhysicalDevice(),
                                        format_, &formatProperties);

    ASSERT(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
           "Device does not support linear blit, can't gen. mips");

    const VkImageAspectFlags aspectMask = isDepth() ?
                                          isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT
                                                      : VK_IMAGE_ASPECT_DEPTH_BIT
                                                    : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image_,
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    int32_t mipWidth = extent_.width;
    int32_t mipHeight = extent_.height;

    for(uint32_t i = 1; i <= mipLevels_; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        if(i == mipLevels_) {
            break;
        }

        const int32_t newMipWidth = mipWidth > 1 ? mipWidth >> 1 : mipWidth;
        const int32_t newMipHeight = mipHeight > 1 ? mipHeight >> 1 : mipHeight;

        VkImageBlit blit {
            .srcSubresource = {
                .aspectMask = aspectMask,
                .mipLevel = i - 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffsets = {
                {
                    0, 0, 0
                },
                {
                    mipWidth, mipHeight, 1
                }
            },
            .dstSubresource = {
                .aspectMask = aspectMask,
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .dstOffsets = {
                {
                    0, 0, 0
                },
                {
                    newMipWidth, newMipHeight, 1
                }
            },
        };

        vkCmdBlitImage(cmdBuffer, image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        mipWidth = newMipWidth;
        mipHeight = newMipHeight;
    }

    const VkImageMemoryBarrier finalBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, // transition needs to complete AFTER
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, // transition needs to complete BEFORE
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // current layout of image
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // target layout of the image
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // important if used by DIFFERENT queue families
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image_,
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    // commandBuffer, srcStageMask, dstStageMask, ...
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &finalBarrier);

    layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    context_.endDebugUtilsLabel(cmdBuffer);
}

void Texture::transitionImageLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout) {

    VkAccessFlags srcAccessMask = VK_ACCESS_NONE; // For the memory barrier
    VkAccessFlags dstAccessMask = VK_ACCESS_NONE;
    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    constexpr VkPipelineStageFlags depthStageMask = 0 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    constexpr VkPipelineStageFlags sampledStageMask = 0 | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    auto oldLayout = layout_;

    if(oldLayout == newLayout) {
        return;
    }

    switch(oldLayout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            break;

        case VK_IMAGE_LAYOUT_GENERAL:
            sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            sourceStage = depthStageMask;
            srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            sourceStage = sampledStageMask;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            sourceStage = VK_PIPELINE_STAGE_HOST_BIT;
            srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            sourceStage = VK_PIPELINE_STAGE_HOST_BIT;
            srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            break;

        default:
            ASSERT(false, "Unknown image layout.");
            break;
    }

    switch(newLayout) {
        case VK_IMAGE_LAYOUT_GENERAL:
        case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
            destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            destinationStage = depthStageMask;
            dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            destinationStage = depthStageMask | sampledStageMask;
            dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_SHADER_READ_BIT |
                            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            destinationStage = sampledStageMask;
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;;
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            // vkQueuePresentKHR does this on its own
            break;

        default:
            ASSERT(false, "Unknown image layout");
            break;
    }

    const VkImageAspectFlags aspectMask = isDepth() ?
                                           isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT
                                                       : VK_IMAGE_ASPECT_DEPTH_BIT
                                           : VK_IMAGE_ASPECT_COLOR_BIT;

    const VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = layout_,
        .newLayout = newLayout,
        .image = image_,
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = mipLevels_,
            .baseArrayLayer = 0,
            .layerCount = multiView_? VK_REMAINING_ARRAY_LAYERS : 1,
        },
    };

    vkCmdPipelineBarrier(cmdBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    layout_ = newLayout;
}

uint32_t Texture::pixelSizeInBytes() const {
    return bytesPerPixel(format_);
}


}
