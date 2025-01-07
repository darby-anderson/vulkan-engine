//
// Created by darby on 7/3/2024.
//

#pragma once

#include "volk.h"
#include <string>

namespace Fairy::Graphics {

class Context;

class Sampler final {

public:
    explicit Sampler(const Context& context, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
                     VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW, float maxLod,
                     const std::string& name = "");

    explicit Sampler(const Context& context, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
                     VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW, float maxLod,
                     bool compareEnable, VkCompareOp compareOp, const std::string& name = "");

    ~Sampler() { vkDestroySampler(device_, sampler_, nullptr); }

    VkSampler vkSampler() const { return sampler_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;

};

}