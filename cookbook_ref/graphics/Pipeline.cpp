//
// Created by darby on 6/5/2024.
//

#include <vector>
#include <array>
#include <mutex>
#include "Pipeline.hpp"
#include "VulkanUtils.hpp"
#include "Context.hpp"
#include "CoreUtils.hpp"
#include "ShaderModule.hpp"

static constexpr int MAX_DESCRIPTOR_SETS = 4096 * 3;

namespace Fairy::Graphics {

VkPipeline Pipeline::pipeline() {
    return vkPipeline_;
}

Pipeline::Pipeline(const Context* context, const Pipeline::GraphicsPipelineDescriptor& desc, VkRenderPass renderPass,
                   const std::string& name) : context_(context), graphicsPipelineDesc_(desc),
                                              bindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS), vkRenderPass_(renderPass),
                                              name_(name) {
    createGraphicsPipeline();
}

void Pipeline::createGraphicsPipeline() {
    const VkSpecializationInfo vertexSpecializationInfo{
        .mapEntryCount =
        static_cast<uint32_t>(graphicsPipelineDesc_.vertexSpecConstants_.size()),
        .pMapEntries = graphicsPipelineDesc_.vertexSpecConstants_.data(),
        .dataSize = !graphicsPipelineDesc_.vertexSpecConstants_.empty()
                    ? graphicsPipelineDesc_.vertexSpecConstants_.back().offset +
                      graphicsPipelineDesc_.vertexSpecConstants_.back().size
                    : 0,
        .pData = graphicsPipelineDesc_.vertexSpecializationData,
    };

    const VkSpecializationInfo fragmentSpecializationInfo{
        .mapEntryCount =
        static_cast<uint32_t>(graphicsPipelineDesc_.fragmentSpecConstants_.size()),
        .pMapEntries = graphicsPipelineDesc_.fragmentSpecConstants_.data(),
        .dataSize = !graphicsPipelineDesc_.fragmentSpecConstants_.empty()
                    ? graphicsPipelineDesc_.fragmentSpecConstants_.back().offset +
                      graphicsPipelineDesc_.fragmentSpecConstants_.back().size
                    : 0,
        .pData = graphicsPipelineDesc_.fragmentSpecializationData,
    };

    const auto vertShader = graphicsPipelineDesc_.vertexShader_.lock();
    ASSERT(vertShader, "Vert Shader destroyed before use in pipeline");
    const auto fragShader = graphicsPipelineDesc_.fragmentShader_.lock();
    ASSERT(vertShader, "Frag Shader destroyed before use in pipeline");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = vertShader->vkShaderStageFlags(),
            .module = vertShader->vkShaderModule(),
            .pName = vertShader->entryPoint().c_str(),
            .pSpecializationInfo = !graphicsPipelineDesc_.vertexSpecConstants_.empty()
                                   ? &vertexSpecializationInfo
                                   : nullptr
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = fragShader->vkShaderStageFlags(),
            .module = fragShader->vkShaderModule(),
            .pName = fragShader->entryPoint().c_str(),
            .pSpecializationInfo = !graphicsPipelineDesc_.fragmentSpecConstants_.empty()
                                   ? &fragmentSpecializationInfo
                                   : nullptr
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = graphicsPipelineDesc_.primitiveTopology,
        .primitiveRestartEnable = VK_FALSE
    };

    const VkViewport viewport = graphicsPipelineDesc_.viewport.toVkViewport();

    const VkRect2D scissor = {
        .offset = {
            .x = 0,
            .y = 0
        },
        .extent = graphicsPipelineDesc_.viewport.toVkExtent()
    };

    const VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    // Fixed Function Setup
    const VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VkCullModeFlags(graphicsPipelineDesc_.cullMode),
        .frontFace = graphicsPipelineDesc_.frontFace,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f, // optional
        .depthBiasClamp = 0.0f,             // optional
        .depthBiasSlopeFactor = 0.0f,       // optional
        .lineWidth = 1.0f
    };

    const VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = graphicsPipelineDesc_.sampleCount,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,           // Optional
        .pSampleMask = nullptr,    // Optional
        .alphaToCoverageEnable = VK_FALSE,    // Optional
        .alphaToOneEnable = VK_FALSE,    // Optional
    };

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;

    if (graphicsPipelineDesc_.blendAttachmentStates_.size() > 0) {
        ASSERT(graphicsPipelineDesc_.blendAttachmentStates_.size() ==
               graphicsPipelineDesc_.colorTextureFormats.size(),
               "Blend states need to be provided for all color textures");
        colorBlendAttachments = graphicsPipelineDesc_.blendAttachmentStates_;
    } else {
        colorBlendAttachments = std::vector<VkPipelineColorBlendAttachmentState>(
            graphicsPipelineDesc_.colorTextureFormats.size(),
            {
                .blendEnable = graphicsPipelineDesc_.blendEnabled,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            });
    }

    const VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY, // optional
        .attachmentCount = uint32_t(colorBlendAttachments.size()),
        .pAttachments = colorBlendAttachments.data(),
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}, // optional
    };

    initDescriptorLayout();

    std::vector<VkDescriptorSetLayout> descSetLayouts(descriptorSetsTypeGroups_.size());
    for (const auto& set: descriptorSetsTypeGroups_) {
        descSetLayouts[set.first] = set.second.vkLayout_;
    }

    vkPipelineLayout_ = createPipelineLayout(descSetLayouts, graphicsPipelineDesc_.pushConstants_);

    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = graphicsPipelineDesc_.depthTestEnable,
        .depthWriteEnable = graphicsPipelineDesc_.depthWriteEnable,
        .depthCompareOp = graphicsPipelineDesc_.depthCompareOperation,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    const VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(graphicsPipelineDesc_.dynamicStates_.size()),
        .pDynamicStates = graphicsPipelineDesc_.dynamicStates_.data()
    };

    const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = uint32_t(graphicsPipelineDesc_.colorTextureFormats.size()),
        .pColorAttachmentFormats = graphicsPipelineDesc_.colorTextureFormats.data(),
        .depthAttachmentFormat = graphicsPipelineDesc_.depthTextureFormat,
        .stencilAttachmentFormat = graphicsPipelineDesc_.stencilTextureFormat
    };

    const VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = graphicsPipelineDesc_.useDynamicRendering_ ? &pipelineRenderingCreateInfo
                                                            : nullptr,

        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &graphicsPipelineDesc_.vertexInputStateCreateInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = vkPipelineLayout_,
        .renderPass = vkRenderPass_,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VK_CHECK(vkCreateGraphicsPipelines(context_->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vkPipeline_));
    context_->setVkObjectName(vkPipeline_, VK_OBJECT_TYPE_PIPELINE, "graphics pipeline " + name_);
}

void Pipeline::initDescriptorLayout() {
    std::vector<SetDescriptor> sets;

    if (bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS) {
        sets = graphicsPipelineDesc_.sets_;
    } else {
        std::cerr << "Other bind points not implemented" << std::endl;
    }

    constexpr VkDescriptorBindingFlags flagsToEnable =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

    for (size_t setIndex = 0; const auto& set: sets) {
        std::vector<VkDescriptorBindingFlags> bindFlags(set.bindings_.size(), flagsToEnable);

        const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<uint32_t>(set.bindings_.size()),
            .pBindingFlags = bindFlags.data()
        };

        const VkDescriptorSetLayoutCreateInfo dslci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
#if defined(_WIN32)
            .pNext = &extendedInfo,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
#endif
            .bindingCount = static_cast<uint32_t>(set.bindings_.size()),
            .pBindings = !set.bindings_.empty() ? set.bindings_.data() : nullptr,
        };

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(context_->device(), &dslci, nullptr, &descriptorSetLayout));

        context_->setVkObjectName(descriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                                  "Graphics pipeline descriptor set " + std::to_string(setIndex++) + "layout: " +
                                  name_);

        descriptorSetsTypeGroups_[set.set_].vkLayout_ = descriptorSetLayout;
    }

}

VkPipelineLayout Pipeline::createPipelineLayout(const std::vector<VkDescriptorSetLayout>& descLayouts,
                                                const std::vector<VkPushConstantRange>& pushConsts) const {
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = (uint32_t) descLayouts.size(),
        .pSetLayouts = descLayouts.data(),
        .pushConstantRangeCount = !pushConsts.empty() ? static_cast<uint32_t>(pushConsts.size()) : 0,
        .pPushConstantRanges = !pushConsts.empty() ? pushConsts.data() : nullptr
    };

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VK_CHECK(vkCreatePipelineLayout(context_->device(), &pipelineLayoutInfo, nullptr, &pipelineLayout));
    context_->setVkObjectName(pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "pipeline layout: " + name_);

    return pipelineLayout;
}

void Pipeline::bind(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, bindPoint_, vkPipeline_);
}

void Pipeline::bindVertexBuffer(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer) {
    VkBuffer vertexBuffers[1] = {vertexBuffer};
    VkDeviceSize vertexOffset[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vertexOffset);
}

void Pipeline::bindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer indexBuffer) {
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Pipeline::allocateDescriptors(const std::vector<SetAndCount>& setAndCount) {
    if (vkDescriptorPool_ == VK_NULL_HANDLE) {
        initDescriptorPool();
    }

    for (auto set: setAndCount) {
        ASSERT(descriptorSetsTypeGroups_.contains(set.set_),
               "This pipeline doesn't have a set with index " + std::to_string(set.set_));

        const VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vkDescriptorPool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetsTypeGroups_[set.set_].vkLayout_
        };

        for (size_t i = 0; i < set.count_; i++) {
            VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
            VK_CHECK(vkAllocateDescriptorSets(context_->device(), &allocInfo, &descriptorSet));
            descriptorSetsTypeGroups_[set.set_].vkSets_.push_back(descriptorSet);

            context_->setVkObjectName(descriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET,
                                      "Descriptor set: " + set.name_ + " " + std::to_string(i));
        }

        for(const auto& n : descriptorSetsTypeGroups_) {
            std::cout << "Key:[" << n.first << "] Value:[ size:" << n.second.vkSets_.size() << " " << n.second.vkLayout_ << "]" << std::endl;
        }

    }
}

void Pipeline::bindDescriptorSets(VkCommandBuffer commandBuffer, const std::vector<SetAndBindingIndex>& sets) {
    for (const auto& set: sets) {
        VkDescriptorSet dSet = descriptorSetsTypeGroups_[set.set].vkSets_[set.bindIdx];
        vkCmdBindDescriptorSets(commandBuffer, bindPoint_, vkPipelineLayout_, set.set, 1u, &dSet, 0, nullptr);
    }
}

void Pipeline::initDescriptorPool() {
    std::vector<SetDescriptor> sets;

    if (bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS) {
        sets = graphicsPipelineDesc_.sets_;
    } else if (bindPoint_ == VK_PIPELINE_BIND_POINT_COMPUTE) {
        ASSERT(false, "Compute binding not created");
    }

    std::vector<VkDescriptorPoolSize> poolSizes;
    for (size_t setIndex = 0; const auto& set: sets) {
        for (const auto& binding: set.bindings_) {
            poolSizes.push_back({binding.descriptorType, MAX_DESCRIPTOR_SETS});
        }
    }

    const VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                 VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = MAX_DESCRIPTOR_SETS,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VK_CHECK(vkCreateDescriptorPool(context_->device(), &descriptorPoolInfo, nullptr, &vkDescriptorPool_));
    context_->setVkObjectName(vkDescriptorPool_, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Pipeline descriptor pool: " + name_);\

}

void Pipeline::updateSamplersDescriptorSets(uint32_t set, uint32_t index, const std::vector<SetBindings>& bindings) {
    ASSERT(!bindings.empty(), "bindings are empty");
    std::vector<std::vector<VkDescriptorImageInfo>> samplerInfo(bindings.size());

    std::vector<VkWriteDescriptorSet> writeDescSets;
    writeDescSets.reserve(bindings.size());

    for (size_t idx = 0; auto& binding: bindings) {
        samplerInfo[idx].reserve(binding.samplers_.size());

        for (const auto& sampler: binding.samplers_) {
            samplerInfo[idx].emplace_back(VkDescriptorImageInfo{
                .sampler = sampler->vkSampler()
            });
        }

        const VkWriteDescriptorSet writeDescSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
            .dstBinding = binding.binding_,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(samplerInfo[idx].size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = samplerInfo[idx].data(),
            .pBufferInfo = nullptr,
        };

        writeDescSets.emplace_back(std::move(writeDescSet));
        idx++;
    }

    vkUpdateDescriptorSets(context_->device(), writeDescSets.size(), writeDescSets.data(), 0, nullptr);
}

void Pipeline::updateTexturesDescriptorSets(uint32_t set, uint32_t index, const std::vector<SetBindings>& bindings) {
    ASSERT(!bindings.empty(), "bindings are empty");
    std::vector<std::vector<VkDescriptorImageInfo>> imageInfo(bindings.size());

    std::vector<VkWriteDescriptorSet> writeDescSets;
    writeDescSets.reserve(bindings.size());

    for (size_t idx = 0; auto& binding: bindings) {
        imageInfo[idx].reserve(binding.textures_.size());

        for (const auto& texture: binding.textures_) {
            imageInfo[idx].emplace_back(VkDescriptorImageInfo{
                .imageView = texture->view(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });
        }

        const VkWriteDescriptorSet writeDescSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
            .dstBinding = binding.binding_,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(imageInfo[idx].size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = imageInfo[idx].data(),
            .pBufferInfo = nullptr,
        };

        writeDescSets.emplace_back(std::move(writeDescSet));
        idx++;
    }

    vkUpdateDescriptorSets(context_->device(), writeDescSets.size(), writeDescSets.data(), 0, nullptr);
}

void Pipeline::updateBuffersDescriptorSets(uint32_t set, uint32_t index, VkDescriptorType type,
                                           const std::vector<SetBindings>& bindings) {
    ASSERT(!bindings.empty(), "bindings are empty");
    std::vector<VkDescriptorBufferInfo> bufferInfo;
    bufferInfo.reserve(bindings.size());

    std::vector<VkWriteDescriptorSet> writeDescSets;
    writeDescSets.reserve(bindings.size());

    for (size_t idx = 0; auto& binding: bindings) {

        bufferInfo.emplace_back(VkDescriptorBufferInfo{
            .buffer = binding.buffer->vkBuffer(),
            .offset = 0,
            .range = binding.bufferBytes
        });

        const VkWriteDescriptorSet writeDescSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
            .dstBinding = binding.binding_,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
        };

        writeDescSets.emplace_back(std::move(writeDescSet));
        idx++;
    }

    vkUpdateDescriptorSets(context_->device(), writeDescSets.size(), writeDescSets.data(), 0, nullptr);
}

void Pipeline::updateDescriptorSets() {
    if (!writeDescSets_.empty()) {
        std::unique_lock<std::mutex> mlock(mutex_);
        vkUpdateDescriptorSets(context_->device(), writeDescSets_.size(), writeDescSets_.data(), 0, nullptr);

        writeDescSets_.clear();
        bufferInfo_.clear();

        bufferViewInfo_.clear();
        imageInfo_.clear();
    }
}

// index is used as a frame-index counter
void
Pipeline::bindBufferResource(uint32_t set, uint32_t binding, uint32_t index, std::shared_ptr<Buffer> buffer,
                             uint32_t offset, uint32_t size, VkDescriptorType type, VkFormat format) {
    bufferInfo_.emplace_back(std::vector<VkDescriptorBufferInfo>{
        VkDescriptorBufferInfo{
            .buffer = buffer->vkBuffer(),
            .offset = offset,
            .range = size
        }
    });

    if (type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
        type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
        ASSERT(format != VK_FORMAT_UNDEFINED, "format must be specified");
        bufferViewInfo_.emplace_back(buffer->requestBufferView(format));
    }

    ASSERT(descriptorSetsTypeGroups_[set].vkSets_[index] != VK_NULL_HANDLE,
           "Need to allocated descriptor set before binding to it.");

    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = nullptr,
        .pBufferInfo = (type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                        type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
                       ? VK_NULL_HANDLE : bufferInfo_.back().data(),
        .pTexelBufferView = (type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                             type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
                            ? &bufferViewInfo_.back() : VK_NULL_HANDLE,
    };

    writeDescSets_.emplace_back(std::move(writeDescSet));
}

void
Pipeline::bindTextureResource(uint32_t set, uint32_t binding, uint32_t index,
                              std::span<std::shared_ptr<Texture>> textures,
                              std::shared_ptr<Sampler> sampler, uint32_t dstArrayElement) {

    if (textures.size() == 0) {
        return;
    }

    std::unique_lock<std::mutex> mlock(mutex_);

    imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
    imageInfo_.back().reserve((textures.size()));

    for (const auto& texture: textures) {
        if (texture) {
            imageInfo_.back().emplace_back(
                VkDescriptorImageInfo{
                    .sampler = sampler ? sampler->vkSampler() : VK_NULL_HANDLE,
                    .imageView = texture->view(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                }
            );
        }
    }

    if (imageInfo_.back().size() == 0) {
        return;
    }

    ASSERT(descriptorSetsTypeGroups_[set].vkSets_[index] != VK_NULL_HANDLE,
           "Did you allocate the descriptor set before binding to it?");

    const VkWriteDescriptorSet writeDescriptorSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
        .dstBinding = binding,
        .dstArrayElement = dstArrayElement,
        .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
        .descriptorType = sampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                  : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = imageInfo_.back().data(),
        .pBufferInfo = nullptr
    };

    writeDescSets_.emplace_back(std::move(writeDescriptorSet));
}

void
Pipeline::bindSamplerResource(uint32_t set, uint32_t binding, uint32_t index,
                              std::span<std::shared_ptr<Sampler>> samplers) {
    imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
    imageInfo_.back().reserve(samplers.size());

    for (const auto& sampler: samplers) {
        imageInfo_.back().emplace_back(
            VkDescriptorImageInfo{
                .sampler = sampler->vkSampler()
            }
        );
    }

    ASSERT(descriptorSetsTypeGroups_[set].vkSets_[index] != VK_NULL_HANDLE,
           "Need to allocate the descriptor set before binding to it.");

    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = imageInfo_.back().data(),
        .pBufferInfo = nullptr
    };

    writeDescSets_.emplace_back(writeDescSet);
}

void Pipeline::bindImageViewResource(int32_t set, uint32_t binding, uint32_t index,
                                     std::span<std::shared_ptr<VkImageView>> imageViews, VkDescriptorType type) {
    imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
    imageInfo_.back().reserve(imageViews.size());
    for (const auto& imview: imageViews) {
        imageInfo_.back().emplace_back(
            VkDescriptorImageInfo{
                .imageView = *imview,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL
            }
        );
    }

    ASSERT(descriptorSetsTypeGroups_[set].vkSets_[index] != VK_NULL_HANDLE,
           "Must allocate the descriptor set before binding to it");

    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
        .descriptorType = type,
        .pImageInfo = imageInfo_.back().data(),
        .pBufferInfo = nullptr
    };

    writeDescSets_.emplace_back(writeDescSet);
}

void
Pipeline::bindBufferResource(uint32_t set, uint32_t binding, uint32_t index,
                             std::vector<std::shared_ptr<Buffer>> buffers,
                             VkDescriptorType type) {
    std::vector<VkDescriptorBufferInfo> bufferInfos;

    for (auto& buffer: buffers) {
        bufferInfos.emplace_back(
            VkDescriptorBufferInfo{
                .buffer = buffer->vkBuffer(),
                .offset = 0,
                .range = buffer->size()
            }
        );
    }

    bufferInfo_.emplace_back(bufferInfos);

    ASSERT(descriptorSetsTypeGroups_[set].vkSets_[index] != VK_NULL_HANDLE,
           "Need to allocate the descriptor set before binding to it.");

    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = uint32_t(bufferInfos.size()),
        .descriptorType = type,
        .pImageInfo = nullptr,
        .pBufferInfo = bufferInfo_.back().data(),
    };

    writeDescSets_.emplace_back(std::move(writeDescSet));
}

void
Pipeline::bindSingleTextureResource(uint32_t set, uint32_t binding, uint32_t index, std::shared_ptr<Texture> texture,
                                    VkDescriptorType type) {
    imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
    imageInfo_.back().push_back(
        VkDescriptorImageInfo{
            .imageView = texture->view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        }
    );

    ASSERT(descriptorSetsTypeGroups_[set].vkSets_[index] != VK_NULL_HANDLE,
           "Need to allocate descriptor set before binding to it.");

    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
        .descriptorType = type,
        .pImageInfo = imageInfo_.back().data(),
        .pBufferInfo = nullptr
    };

    writeDescSets_.emplace_back(writeDescSet);
}

void Pipeline::bindSingleTextureAndSamplerResource(uint32_t set, uint32_t binding, uint32_t index,
                                                   std::shared_ptr<Texture> texture,
                                                   std::shared_ptr<Sampler> sampler, VkDescriptorType type) {

    imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
    imageInfo_.back().push_back(
        VkDescriptorImageInfo{
            .sampler = sampler->vkSampler(),
            .imageView = texture->view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        }
    );

    ASSERT(descriptorSetsTypeGroups_[set].vkSets_[index] != VK_NULL_HANDLE,
           "Need to allocate the descriptor set before binding");

    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSetsTypeGroups_[set].vkSets_[index],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
        .descriptorType = type,
        .pImageInfo = imageInfo_.back().data(),
        .pBufferInfo = nullptr
    };

    writeDescSets_.emplace_back(writeDescSet);
}

void Pipeline::updatePushConstants(VkCommandBuffer commandBuffer, VkShaderStageFlags flags, uint32_t size,
                                   const void* data) {
    vkCmdPushConstants(commandBuffer, vkPipelineLayout_, flags, 0, size, data);
}

}