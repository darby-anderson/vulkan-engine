//
// Created by darby on 7/3/2024.
//

#include "Sampler.hpp"
#include "Context.hpp"

namespace Fairy::Graphics {

Sampler::Sampler(const Context& context, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
                 VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW, float maxLod,
                 const std::string& name)
    : device_{context.device()} {

    const VkSamplerCreateInfo samplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = magFilter,
        .minFilter = minFilter,
        .mipmapMode = maxLod > 0
                      ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                      : VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = addressModeU,
        .addressModeV = addressModeV,
        .addressModeW = addressModeW,
        .mipLodBias = 0,
        .anisotropyEnable = VK_FALSE,
        .minLod = 0,
        .maxLod = maxLod,
    };

    VK_CHECK(vkCreateSampler(device_, &samplerCreateInfo, nullptr, &sampler_));
    context.setVkObjectName(sampler_, VK_OBJECT_TYPE_SAMPLER, name);
}

Sampler::Sampler(const Context& context, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
                 VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW, float maxLod, bool compareEnable,
                 VkCompareOp compareOp, const std::string& name) : device_{context.device()} {

    const VkSamplerCreateInfo samplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = magFilter,
        .minFilter = minFilter,
        .mipmapMode = maxLod > 0
                      ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                      : VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = addressModeU,
        .addressModeV = addressModeV,
        .addressModeW = addressModeW,
        .mipLodBias = 0,
        .anisotropyEnable = VK_FALSE,
        .compareEnable = compareEnable,
        .compareOp = compareOp,
        .minLod = 0,
        .maxLod = maxLod,
    };

    VK_CHECK(vkCreateSampler(device_, &samplerCreateInfo, nullptr, &sampler_));
    context.setVkObjectName(sampler_, VK_OBJECT_TYPE_SAMPLER, name);

}

}