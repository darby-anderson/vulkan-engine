//
// Created by darby on 6/5/2024.
//

#pragma once

#include <memory>
#include <unordered_map>
#include <span>
#include <list>

#include "volk.h"
#include "Sampler.hpp"
#include "Buffer.hpp"

namespace Fairy::Graphics {

class Context;

class ShaderModule;

class Buffer;

class Pipeline final {

public:

    struct SetDescriptor {
        uint32_t set_;
        std::vector<VkDescriptorSetLayoutBinding> bindings_;
    };

    struct Viewport {

        Viewport(const VkExtent2D& extent) { viewport_ = fromExtent(extent); }

        Viewport() = default;

        Viewport(const Viewport&) = default;

        Viewport& operator=(const Viewport&) = default;

        Viewport(const VkViewport& viewport) : viewport_(viewport) {};

        Viewport& operator=(const VkViewport& viewport) {
            viewport_ = viewport;
            return *this;
        }

        Viewport& operator=(const VkExtent2D& extents) {
            viewport_ = fromExtent(extents);
            return *this;
        }

        VkExtent2D toVkExtent() const {
            return VkExtent2D{static_cast<uint32_t>(std::abs(viewport_.width)),
                              static_cast<uint32_t>(std::abs(viewport_.height))};
        }

        VkViewport toVkViewport() const { return viewport_; }

    private:
        VkViewport fromExtent(const ::VkExtent2D& extent) {
            VkViewport v;
            v.x = 0;
            v.y = 0;
            v.width = extent.width;
            v.height = extent.height;
            v.minDepth = 0.0;
            v.maxDepth = 1.0;

            return v;
        }

        VkViewport viewport_ = {};
    };

    struct GraphicsPipelineDescriptor {
        std::vector<SetDescriptor> sets_;
        std::weak_ptr<ShaderModule> vertexShader_;
        std::weak_ptr<ShaderModule> fragmentShader_;
        std::vector<VkPushConstantRange> pushConstants_;
        std::vector<VkDynamicState> dynamicStates_;
        bool useDynamicRendering_ = false;
        std::vector<VkFormat> colorTextureFormats;
        VkFormat depthTextureFormat = VK_FORMAT_UNDEFINED;
        VkFormat stencilTextureFormat = VK_FORMAT_UNDEFINED;

        VkPrimitiveTopology primitiveTopology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
        VkCullModeFlagBits cullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        Viewport viewport;
        bool blendEnabled = false;
        uint32_t numberBlendAttachments = 0u;
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        VkCompareOp depthCompareOperation = VK_COMPARE_OP_LESS;

        VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            .vertexAttributeDescriptionCount = 0
        };
        std::vector<VkSpecializationMapEntry> vertexSpecConstants_;
        std::vector<VkSpecializationMapEntry> fragmentSpecConstants_;
        void* vertexSpecializationData = nullptr;
        void* fragmentSpecializationData = nullptr;

        std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates_;

    };

    explicit Pipeline(const Context* context, const GraphicsPipelineDescriptor& desc, VkRenderPass renderPass,
                      const std::string& name = "");

    ~Pipeline() = default;

    VkPipeline pipeline();

    void bind(VkCommandBuffer commandBuffer);

    void bindVertexBuffer(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer);

    void bindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer indexBuffer);

    struct SetAndCount {
        uint32_t set_;
        uint32_t count_;
        std::string name_;
    };

    void allocateDescriptors(const std::vector<SetAndCount>& setAndCount);

    struct SetAndBindingIndex {
        uint32_t set;
        uint32_t bindIdx;
    };

    void bindDescriptorSets(VkCommandBuffer commandBuffer, const std::vector<SetAndBindingIndex>& sets);

    struct SetBindings {
        uint32_t set_ = 0;
        uint32_t binding_ = 0;
        std::span<std::shared_ptr<Texture>> textures_;
        std::span<std::shared_ptr<Sampler>> samplers_;
        std::shared_ptr<Buffer> buffer;
        uint32_t index_ = 0;
        uint32_t offset_ = 0;
        VkDeviceSize bufferBytes = 0;
    };

    void updateSamplersDescriptorSets(uint32_t set, uint32_t index,
                                      const std::vector<SetBindings>& bindings);

    void updateTexturesDescriptorSets(uint32_t set, uint32_t index,
                                      const std::vector<SetBindings>& bindings);

    void updateBuffersDescriptorSets(uint32_t set, uint32_t index, VkDescriptorType type,
                                     const std::vector<SetBindings>& bindings);

    void updateDescriptorSets();

    void updatePushConstants(VkCommandBuffer commandBuffer, VkShaderStageFlags flags,
                             uint32_t size, const void* data);

    // Assigns recource to a position in the resource array
    // specific to the resource's type
    void bindBufferResource(uint32_t set, uint32_t binding, uint32_t index,
                            std::shared_ptr<Buffer> buffer, uint32_t offset, uint32_t size,
                            VkDescriptorType type, VkFormat format = VK_FORMAT_UNDEFINED);

    void bindTextureResource(uint32_t set, uint32_t binding, uint32_t index,
                             std::span<std::shared_ptr<Texture>> textures,
                             std::shared_ptr<Sampler> sampler = nullptr,
                             uint32_t dstArrayElement = 0);

    void bindSamplerResource(uint32_t set, uint32_t binding, uint32_t index,
                             std::span<std::shared_ptr<Sampler>> samplers);

    void bindImageViewResource(int32_t set, uint32_t binding, uint32_t index,
                               std::span<std::shared_ptr<VkImageView>> imageViews,
                               VkDescriptorType type);

    void bindBufferResource(uint32_t set, uint32_t binding, uint32_t index,
                            std::vector<std::shared_ptr<Buffer>> buffers, VkDescriptorType type);

    void bindSingleTextureResource(uint32_t set, uint32_t binding, uint32_t index,
                                   std::shared_ptr<Texture> texture, VkDescriptorType type);

    void bindSingleTextureAndSamplerResource(uint32_t set, uint32_t binding, uint32_t index,
                                             std::shared_ptr<Texture> texture, std::shared_ptr<Sampler> sampler,
                                             VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

private:
    void createGraphicsPipeline();

    void initDescriptorLayout();

    void initDescriptorPool();

    VkPipelineLayout createPipelineLayout(const std::vector<VkDescriptorSetLayout>& descLayouts,
                                          const std::vector<VkPushConstantRange>& pushConsts) const;

    VkPipeline vkPipeline_;

    const Context* context_;
    const GraphicsPipelineDescriptor& graphicsPipelineDesc_;
    VkPipelineBindPoint bindPoint;
    VkRenderPass vkRenderPass_;

    VkPipelineBindPoint bindPoint_ = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipelineLayout vkPipelineLayout_ = VK_NULL_HANDLE;

    // Multiple descriptor sets can use the same layout
    struct DescriptorSetTypeGroup {
        std::vector<VkDescriptorSet> vkSets_;
        VkDescriptorSetLayout vkLayout_ = VK_NULL_HANDLE;
    };
    std::unordered_map<uint32_t, DescriptorSetTypeGroup> descriptorSetsTypeGroups_;
    VkDescriptorPool vkDescriptorPool_ = VK_NULL_HANDLE;

    std::list<std::vector<VkDescriptorBufferInfo>> bufferInfo_;
    std::list<VkBufferView> bufferViewInfo_;
    std::list<std::vector<VkDescriptorImageInfo>> imageInfo_;

    std::vector<VkWriteDescriptorSet> writeDescSets_;
    std::mutex mutex_;

    const std::string& name_;

};

}