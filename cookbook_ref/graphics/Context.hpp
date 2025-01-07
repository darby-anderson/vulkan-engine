//
// Created by darby on 5/28/2024.
//

#pragma once

#include "PhysicalDevice.hpp"
#include "vk_mem_alloc.h"
#include "CoreUtils.hpp"
#include "Swapchain.hpp"
#include "ShaderModule.hpp"
#include "Pipeline.hpp"
#include "CommandQueueManager.hpp"
#include "RenderPass.hpp"
#include "VulkanUtils.hpp"
#include "Framebuffer.hpp"
#include "Buffer.hpp"

#include <unordered_set>
#include <vector>
#include <any>
#include <utility>
#include <memory>
#include <glm/vec4.hpp>

namespace Fairy::Graphics {

class RenderPass;
class CommandQueueManager;
class Swapchain;
class Buffer;

template<size_t CHAIN_SIZE = 10>
class VulkanFeatureChain {
public:
    VulkanFeatureChain() = default;

    auto& pushBack(auto nextVulkanChainStruct) {
        ASSERT(currentIndex_ < CHAIN_SIZE, "Chain is full");
        data_[currentIndex_] = nextVulkanChainStruct;

        auto& next = std::any_cast<decltype(nextVulkanChainStruct)&>(data_[currentIndex_]);

        next.pNext = std::exchange(firstNext_, &next);
        currentIndex_++;

        return next;
    }

    [[nodiscard]] void* firstNextPtr() const { return firstNext_; }

private:
    std::array<std::any, CHAIN_SIZE> data_;
    VkBaseInStructure* root_ = nullptr;
    int currentIndex_ = 0;
    void* firstNext_ = VK_NULL_HANDLE;
};

class Context final {

public:
    // MOVABLE_ONLY(Context);

    std::unordered_set<std::string> enabledLayers;
    std::unordered_set<std::string> enabledInstanceExtensions;

    std::vector<std::string> queryInstanceLayers();

    std::vector<std::string> queryInstanceExtensions();



    Context(void* window,
        const std::vector<std::string>& requestedLayers,
        const std::vector<std::string>& requestedInstanceExtensions,
        const std::vector<std::string>& requestedDeviceExtensions,
        VkQueueFlags requestedQueueTypes,
        bool printEnums,
        const std::string& name);

    static void enableDefaultFeatures();

    static void enabledBufferDeviceAddressFeature();

    static void enableDynamicRenderingFeature();

    template<typename T>
    void setVkObjectName(T handle, VkObjectType type, const std::string& name) const {
#if defined (VK_EXT_debug_utils)
        if (enabledInstanceExtensions.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != enabledInstanceExtensions.end()) {
            const VkDebugUtilsObjectNameInfoEXT info = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = type,
                .objectHandle = reinterpret_cast<uint64_t>(handle),
                .pObjectName = name.c_str()
            };

            VK_CHECK(vkSetDebugUtilsObjectNameEXT(device_, &info));
        }
#else
        (void)handle; // tell compiler to ignore that these are not being used
        (void)type;
        (void)name;
#endif
    }

    std::vector<std::pair<uint32_t, uint32_t>> queueFamilyIndexAndCount() const;

    void beginDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name, const glm::vec4& color) const;
    void endDebugUtilsLabel(VkCommandBuffer commandBuffer) const;

    std::shared_ptr<Sampler> createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
                                           VkSamplerAddressMode addressModeV,
                                           VkSamplerAddressMode addressModeW, float maxLod,
                                           const std::string& name = "") const;

    std::shared_ptr<Sampler> createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
                                           VkSamplerAddressMode addressModeV,
                                           VkSamplerAddressMode addressModeW, float maxLod, bool compareEnable,
                                           VkCompareOp compareOp,
                                           const std::string& name = "") const;

    void createSwapchain(VkFormat imageFormat, VkColorSpaceKHR colorSpace, VkPresentModeKHR presentMode,
                         const VkExtent2D& extent);

    std::shared_ptr<ShaderModule>
    createShaderModule(const std::string& filePath, VkShaderStageFlagBits stages, const std::string& name = "main");

    std::shared_ptr<RenderPass> createRenderPass(const std::vector<std::shared_ptr<Texture>>& attachments,
                                                 const std::vector<VkAttachmentLoadOp>& loadOp,
                                                 const std::vector<VkAttachmentStoreOp>& storeOp,
                                                 const std::vector<VkImageLayout>& layout,
                                                 VkPipelineBindPoint bindPoint) const;

    std::shared_ptr<Pipeline>
    createGraphicsPipeline(const Pipeline::GraphicsPipelineDescriptor& graphicsPipelineDescriptor,
                           VkRenderPass renderPass, const std::string& name = "");

    std::shared_ptr<CommandQueueManager>
    createGraphicsCommandQueue(uint32_t imageCount, uint32_t concurrentNumCommands, const std::string& name = "",
                               int graphicsQueueIndex = -1);

    std::shared_ptr<Texture> createTexture(VkImageType type, VkFormat format, VkImageCreateFlags flags,
                                           VkImageUsageFlags usageFlags, VkExtent3D extents, uint32_t numMipLevels,
                                           uint32_t layerCount, VkMemoryPropertyFlags memoryFlags, bool generateMips = false,
                                           VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT, const std::string& name = "") const;

    std::unique_ptr<Framebuffer>
    createFramebuffer(VkRenderPass renderPass, std::vector<std::shared_ptr<Texture>> colorAttachments,
                      std::shared_ptr<Texture> depthAttachment, std::shared_ptr<Texture> stencilAttachment,
                      const std::string& name = "") const;

    std::shared_ptr<Buffer>
    createBuffer(size_t size, VkBufferUsageFlags flags, VmaMemoryUsage memUsage, const std::string& name) const;

    std::shared_ptr<Buffer>
    createPersistentBuffer(size_t size, VkBufferUsageFlags usage, const std::string& name) const;

    std::shared_ptr<Buffer> createStagingBuffer(VkDeviceSize size, const std::string& name) const;

    std::shared_ptr<Buffer> createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage, Buffer* actualBuffer,
                                                const std::string& name) const;

    void uploadToGPUBuffer(CommandQueueManager& queueMgr, VkCommandBuffer commandBuffer, Buffer* gpuBuffer,
                           const void* data, long totalSize, uint64_t gpuBufferOffset = 0) const;

    VkInstance instance() const { return instance_; }

    VkDevice device() const;

    VkQueue graphicsQueue(int index = 0) const { return graphicsQueues_[index]; }

    VmaAllocator allocator() const;

    PhysicalDevice physicalDevice() const { return physicalDevice_; }

    Swapchain* swapchain() const { return swapchain_.get(); }

private:
    const uint32_t apiVersion = VK_API_VERSION_1_3;

    const VkApplicationInfo applicationInfo_ = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan Engine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };
    bool printEnums_ = true;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VkDevice device_ = VK_NULL_HANDLE;

    VmaAllocator vmaAllocator_;
    VmaAllocation vmaAllocation_;

    VkQueue presentationQueue_ = VK_NULL_HANDLE;

    std::vector<VkQueue> graphicsQueues_;
    std::vector<VkQueue> computeQueues_;
    std::vector<VkQueue> transferQueues_;
    std::vector<VkQueue> sparseQueues_;

    VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPool computeCommandPool = VK_NULL_HANDLE;
    VkCommandPool presentationCommandPool = VK_NULL_HANDLE;
    VkCommandPool transferCommandPool = VK_NULL_HANDLE;
    VkCommandPool sparseCommandPool = VK_NULL_HANDLE;

    static VkPhysicalDeviceFeatures physicalDeviceFeatures_;
    static VkPhysicalDeviceVulkan11Features enable11Features_;
    static VkPhysicalDeviceVulkan12Features enable12Features_;
    static VkPhysicalDeviceVulkan13Features enable13Features_;

    PhysicalDevice physicalDevice_;
    std::unique_ptr<Swapchain> swapchain_;

    void* window_handle = nullptr;

#if defined(VK_EXT_debug_utils)
    VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
#endif

    void createVkDevice(VkPhysicalDevice vkPhysicalDevice, const std::vector<std::string>& requestedDeviceExtensions,
                        VkQueueFlags requestedQueueTypes, const std::string& name);

    std::vector<PhysicalDevice> queryPhysicalDevices(const std::vector<std::string> requestedExtensions) const;

    void createVMAAllocator();

};

}