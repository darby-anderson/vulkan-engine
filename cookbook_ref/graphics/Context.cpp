//
// Created by darby on 5/28/2024.
//

#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <functional>

#define VK_USE_PLATFORM_WIN32_KHR

#include "volk.h"

#define VMA_IMPLEMENTATION

#include "vk_mem_alloc.h"

#include "VulkanUtils.hpp"
#include "Context.hpp"
#include "CoreUtils.hpp"
#include "Swapchain.hpp"
#include "Framebuffer.hpp"


namespace {
#if defined(VK_EXT_debug_utils)

VkBool32 VKAPI_PTR debugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
        LOGE("debugMessengerCallback : MessageCode is {%s} & Message is {%s}",
             pCallbackData->pMessageIdName, pCallbackData->pMessage);
#if defined(_WIN32)
        // __debugbreak();
#else
        raise(SIGTRAP);
#endif
    } else if (messageSeverity & (~VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
        LOGW("debugMessengerCallback : MessageCode is {%s} & Message is {%s}",
             pCallbackData->pMessageIdName, pCallbackData->pMessage);
    } else {
        LOGI("debugMessengerCallback : MessageCode is {%s} & Message is {%s}",
             pCallbackData->pMessageIdName, pCallbackData->pMessage);
    }

    return VK_FALSE;
}

#endif
}  // namespace

namespace Fairy::Graphics {

VkPhysicalDeviceFeatures Context::physicalDeviceFeatures_ = {
    .independentBlend = VK_TRUE,
    .vertexPipelineStoresAndAtomics = VK_TRUE,
    .fragmentStoresAndAtomics = VK_TRUE,
};

VkPhysicalDeviceVulkan11Features Context::enable11Features_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
};

VkPhysicalDeviceVulkan12Features Context::enable12Features_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
};

VkPhysicalDeviceVulkan13Features Context::enable13Features_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
};


std::vector<std::string> Context::queryInstanceLayers() {
    uint32_t instanceLayerCount{0};
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));

    std::vector<VkLayerProperties> layers(instanceLayerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, layers.data()));

    // transfer from VkLayerProperties container into string container
    std::vector<std::string> availableLayers;
    std::transform(
        layers.begin(), layers.end(),
        std::back_inserter(availableLayers),
        [](const VkLayerProperties& properties) {
            return properties.layerName;
        });

    if (printEnums_) {
        std::cout << "Found " << instanceLayerCount << " layers for this instance" << std::endl;
        for (const auto& layer: availableLayers) {
            std::cout << layer << std::endl;
        }
    }

    return availableLayers;
}

std::vector<std::string> Context::queryInstanceExtensions() {
    uint32_t instanceExtensionCount{0};
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));

    std::vector<VkExtensionProperties> extensions(instanceExtensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, extensions.data()));

    // transfer from VkLayerProperties container into string container
    std::vector<std::string> availableExtensions;
    std::transform(
        extensions.begin(), extensions.end(),
        std::back_inserter(availableExtensions),
        [](const VkExtensionProperties& properties) {
            return properties.extensionName;
        });

    if (printEnums_) {
        std::cout << "Found " << instanceExtensionCount << " extensions for this instance" << std::endl;
        for (const auto& layer: availableExtensions) {
            std::cout << layer << std::endl;
        }
    }

    return availableExtensions;
}

std::vector<PhysicalDevice> Context::queryPhysicalDevices(const std::vector<std::string> requestedExtensions) const {

    std::cout << "2.25" << std::endl;

    uint32_t deviceCount{0};
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr));

    std::cout << "2.5" << std::endl;

    ASSERT(deviceCount > 0, "No vulkan-enabled devices found");

    std::vector<VkPhysicalDevice> vulkanPhysicalDevices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, vulkanPhysicalDevices.data()));

    std::cout << "3" << std::endl;

    std::vector<PhysicalDevice> physicalDevices;
    for (const auto device: vulkanPhysicalDevices) {
        physicalDevices.emplace_back(PhysicalDevice(device, surface_, requestedExtensions, printEnums_));
    }

    return physicalDevices;
}



Context::Context(void* window, const std::vector<std::string>& requestedLayers,
                 const std::vector<std::string>& requestedInstanceExtensions,
                 const std::vector<std::string>& requestedDeviceExtensions,
                 VkQueueFlags requestedQueueTypes, bool printEnums,
                 const std::string& name) : printEnums_{printEnums} {

    this->window_handle = window;

    enabledLayers = Fairy::Graphics::filterExtensions(queryInstanceLayers(), requestedLayers);
    enabledInstanceExtensions = Fairy::Graphics::filterExtensions(queryInstanceExtensions(), requestedInstanceExtensions);

    // Pass layers to enable to Vulkan
    std::vector<const char*> instanceLayers(enabledLayers.size());
    // std::mem_fn wraps our function so it can be called as unary operator
    std::transform(enabledLayers.begin(), enabledLayers.end(),
                   instanceLayers.begin(), std::mem_fn(&std::string::c_str));
    std::cout << "1" << std::endl;

    std::vector<const char*> instanceExtensions(enabledInstanceExtensions.size());
    std::transform(enabledInstanceExtensions.begin(), enabledInstanceExtensions.end(),
                   instanceExtensions.begin(), std::mem_fn(&std::string::c_str));
    std::cout << "2" << std::endl;

    const VkApplicationInfo applicationInfo_ = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "VulkanEngine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    /*const VkValidationFeaturesEXT features = {
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
#if defined(VK_EXT_layer_settings)
        .pNext = DEBUG_SHADER_PRINTF_CALLBACK ? &layer_settings_create_info : nullptr,
#endif

    };*/

    const VkInstanceCreateInfo instanceInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &applicationInfo_,
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data()
    };

    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance_));
    std::cout << "Vulkan Instance Created" << std::endl;
    volkLoadInstance(instance_);

#if defined(VK_EXT_debug_utils)
    if (enabledInstanceExtensions.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
#if defined(VK_EXT_device_address_binding_report)
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
#endif
            ,
            .pfnUserCallback = &debugMessengerCallback,
            .pUserData = nullptr,
        };
        VK_CHECK(
            vkCreateDebugUtilsMessengerEXT(instance_, &messengerInfo, nullptr, &messenger_));
    }
#endif

#if defined (VK_USE_PLATFORM_WIN32_KHR) && defined (VK_KHR_win32_surface)
    if (enabledInstanceExtensions.contains(VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
        if (window != nullptr) {
            const VkWin32SurfaceCreateInfoKHR winCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hinstance = GetModuleHandle(NULL),
                .hwnd = (HWND) window
            };
            VK_CHECK(vkCreateWin32SurfaceKHR(instance_, &winCreateInfo, nullptr, &surface_));
        }
    }
#endif

    physicalDevice_ = queryPhysicalDevices(requestedDeviceExtensions)[0]; // Use first phys device for now. TODO: implement device-choosing algo
    physicalDevice_.reserveQueues(requestedQueueTypes | VK_QUEUE_GRAPHICS_BIT);

    createVkDevice(physicalDevice_.getPhysicalDevice(), requestedDeviceExtensions, requestedQueueTypes, "Device");
    std::cout << "Vulkan Device Created" << std::endl;

    createVMAAllocator();
}

void Context::createVkDevice(VkPhysicalDevice vkPhysicalDevice, const std::vector<std::string>& requestedDeviceExtensions,
                        VkQueueFlags requestedQueueTypes, const std::string& name) {
    // LOGICAL DEVICE CREATION
    auto familyIndicesAndQueueCounts = physicalDevice_.getReservedFamiliesAndQueueCounts();
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::vector<std::vector<float>> queueFamilyPriorities(familyIndicesAndQueueCounts.size());
    for (size_t index = 0; const auto& [queueFamilyIndex, queueCount]: familyIndicesAndQueueCounts) {
        queueFamilyPriorities[index] = std::vector<float>(queueCount, 1.0f);

        queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = queueCount,
            .pQueuePriorities = queueFamilyPriorities[index].data()
        });
        index++;
    }

    std::vector<const char*> deviceExtensions(requestedDeviceExtensions.size());
    std::transform(
        requestedDeviceExtensions.begin(),
        requestedDeviceExtensions.end(),
        deviceExtensions.begin(),
        std::mem_fn(&std::string::c_str)
    );

    const auto familyIndices = physicalDevice_.getReservedFamiliesAndQueueCounts();

    const VkPhysicalDeviceFeatures2 deviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = physicalDeviceFeatures_,
    };

    VulkanFeatureChain<> featureChain;

    featureChain.pushBack(deviceFeatures);

    featureChain.pushBack(enable11Features_);
    featureChain.pushBack(enable12Features_);
    featureChain.pushBack(enable13Features_);

    // Check if features desired in feature chain are available
    VkPhysicalDeviceVulkan13Features deviceVulkan13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };

    VkPhysicalDeviceVulkan12Features deviceVulkan12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &deviceVulkan13Features,
    };

    VkPhysicalDeviceVulkan11Features deviceVulkan11Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &deviceVulkan12Features
    };

    VkPhysicalDeviceFeatures2 deviceFeaturesQuery = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &deviceVulkan11Features
    };
    vkGetPhysicalDeviceFeatures2(physicalDevice_.getPhysicalDevice(), &deviceFeaturesQuery);

    // 1.0 Features
    // .independentBlend = VK_TRUE,
    // .vertexPipelineStoresAndAtomics = VK_TRUE,
    // .fragmentStoresAndAtomics = VK_TRUE,
    {
        if(deviceFeaturesQuery.features.independentBlend != VK_TRUE) {
            std::cerr << "independentBlend not supported on physical device" << std::endl;
        }

        if(deviceFeaturesQuery.features.vertexPipelineStoresAndAtomics != VK_TRUE) {
            std::cerr << "vertexPipelineStoresAndAtomics not supported on physical device" << std::endl;
        }

        if(deviceFeaturesQuery.features.fragmentStoresAndAtomics != VK_TRUE) {
            std::cerr << "fragmentStoresAndAtomics not supported on physical device" << std::endl;
        }
    }

    // 1.1 Features
    // -> None currently

    // 1.2 Features
    // shaderSampledImageArrayNonUniformIndexing
    // shaderStorageImageArrayNonUniformIndexing
    // descriptorBindingSampledImageUpdateAfterBind
    // descriptorBindingStorageBufferUpdateAfterBind
    // descriptorBindingUpdateUnusedWhilePending
    // descriptorBindingPartiallyBound
    // descriptorBindingVariableDescriptorCount
    // descriptorIndexing
    // runtimeDescriptorArray
    // bufferDeviceAddress
    // bufferDeviceAddressCaptureReplay
    {
        if(deviceVulkan12Features.shaderSampledImageArrayNonUniformIndexing != VK_TRUE) {
            std::cerr << "shaderSampledImageArrayNonUniformIndexing not supported on physical device" << std::endl;
        }

        if(deviceVulkan12Features.shaderStorageImageArrayNonUniformIndexing != VK_TRUE) {
            std::cerr << "shaderStorageImageArrayNonUniformIndexing not supported on physical device" << std::endl;
        }

        if(deviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind != VK_TRUE) {
            std::cerr << "descriptorBindingSampledImageUpdateAfterBind not supported on physical device" << std::endl;
        }

        if(deviceVulkan12Features.descriptorBindingStorageBufferUpdateAfterBind != VK_TRUE) {
            std::cerr << "descriptorBindingStorageBufferUpdateAfterBind not supported on physical device" << std::endl;
        }

        if(deviceVulkan12Features.descriptorBindingUpdateUnusedWhilePending != VK_TRUE) {
            std::cerr << "descriptorBindingUpdateUnusedWhilePending not supported on physical device" << std::endl;
        }

        if(deviceVulkan12Features.descriptorBindingPartiallyBound != VK_TRUE) {
            std::cerr << "descriptorBindingPartiallyBound not supported on physical device" << std::endl;
        }

         if(deviceVulkan12Features.descriptorBindingVariableDescriptorCount != VK_TRUE) {
            std::cerr << "descriptorBindingVariableDescriptorCount not supported on physical device" << std::endl;
        }

        if(deviceVulkan12Features.descriptorIndexing != VK_TRUE) {
            std::cerr << "descriptorIndexing not supported on physical device" << std::endl;
        }

        if(deviceVulkan12Features.runtimeDescriptorArray != VK_TRUE) {
            std::cerr << "runtimeDescriptorArray not supported on physical device" << std::endl;
        }

        if(deviceVulkan12Features.bufferDeviceAddress != VK_TRUE) {
            std::cerr << "bufferDeviceAddress not supported on physical device" << std::endl;
            std::cerr << "^^ this is okay, we're not using it right now" << std::endl;
        }

        if(deviceVulkan12Features.bufferDeviceAddressCaptureReplay != VK_TRUE) {
            std::cerr << "bufferDeviceAddressCaptureReplay not supported on physical device" << std::endl;
            std::cerr << "^^ this is okay, we're not using it right now" << std::endl;
        }
    }

    // 1.3 Features
    // dynamicRendering
    {
        if(deviceVulkan13Features.dynamicRendering != VK_TRUE) {
            std::cerr << "dynamicRendering not supported on physical device" << std::endl;
        }
    }



    const VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = featureChain.firstNextPtr(),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        // .enabledLayerCount = ?? deprecated and ignored
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };

    VK_CHECK(vkCreateDevice(physicalDevice_.getPhysicalDevice(), &dci, nullptr, &device_));

    volkLoadDevice(device_);

    // Get Device Queues
    if (physicalDevice_.getGraphicsFamilyIndex().has_value()) {
        if (physicalDevice_.getGraphicsQueueCount() > 0) {
            graphicsQueues_.resize(physicalDevice_.getGraphicsQueueCount(), VK_NULL_HANDLE);

            for (int i = 0; i < graphicsQueues_.size(); i++) {
                vkGetDeviceQueue(device_, physicalDevice_.getGraphicsFamilyIndex().value(), i, &graphicsQueues_[i]);
            }
        }
    }

    if (physicalDevice_.getComputeFamilyIndex().has_value()) {
        if (physicalDevice_.getComputeQueueCount() > 0) {
            computeQueues_.resize(physicalDevice_.getComputeQueueCount(), VK_NULL_HANDLE);

            for (int i = 0; i < computeQueues_.size(); i++) {
                vkGetDeviceQueue(device_, physicalDevice_.getComputeFamilyIndex().value(), i, &computeQueues_[i]);
            }
        }
    }

    if (physicalDevice_.getPresentationFamilyIndex().has_value()) {
        vkGetDeviceQueue(device_, physicalDevice_.getPresentationFamilyIndex().value(), 0, &presentationQueue_);
    }

    if (physicalDevice_.getTransferFamilyIndex().has_value()) {
        if (physicalDevice_.getTransferQueueCount() > 0) {
            transferQueues_.resize(physicalDevice_.getTransferQueueCount(), VK_NULL_HANDLE);

            for (int i = 0; i < transferQueues_.size(); i++) {
                vkGetDeviceQueue(device_, physicalDevice_.getTransferFamilyIndex().value(), i, &transferQueues_[i]);
            }
        }
    }

    if (physicalDevice_.getSparseFamilyIndex().has_value()) {
        if (physicalDevice_.getSparseQueueCount() > 0) {
            sparseQueues_.resize(physicalDevice_.getSparseQueueCount(), VK_NULL_HANDLE);

            for (int i = 0; i < sparseQueues_.size(); i++) {
                vkGetDeviceQueue(device_, physicalDevice_.getSparseFamilyIndex().value(), i, &sparseQueues_[i]);
            }
        }
    }
}

VkDevice Context::device() const {
    return device_;
}

VmaAllocator Context::allocator() const {
    return vmaAllocator_;
}

void Context::enableDefaultFeatures() {
    enable12Features_.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    enable12Features_.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    enable12Features_.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    enable12Features_.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    enable12Features_.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    enable12Features_.descriptorBindingPartiallyBound = VK_TRUE;
    enable12Features_.descriptorBindingVariableDescriptorCount = VK_TRUE;
    enable12Features_.descriptorIndexing = VK_TRUE;
    enable12Features_.runtimeDescriptorArray = VK_TRUE;
}

void Context::enabledBufferDeviceAddressFeature() {
    // enable12Features_.bufferDeviceAddress = VK_TRUE;
    // enable12Features_.bufferDeviceAddressCaptureReplay = VK_TRUE; // not available on my machine, but required to work with Renderdoc while bufferDeviceAddress is being used
}

void Context::enableDynamicRenderingFeature() {
    enable13Features_.dynamicRendering = VK_TRUE;
}


void Context::createSwapchain(VkFormat imageFormat, VkColorSpaceKHR colorSpace, VkPresentModeKHR presentMode,
                              const VkExtent2D& extent) {

    swapchain_ = std::make_unique<Swapchain>(*this, physicalDevice_, surface_, presentationQueue_, imageFormat,
                                             colorSpace, presentMode, extent, "");

}

std::shared_ptr<ShaderModule>
Context::createShaderModule(const std::string& filePath, VkShaderStageFlagBits stages, const std::string& name) {
    return std::make_shared<ShaderModule>(this, filePath, stages, name);
}

std::shared_ptr<RenderPass> Context::createRenderPass(const std::vector<std::shared_ptr<Texture>>& attachments,
                                                      const std::vector<VkAttachmentLoadOp>& loadOp,
                                                      const std::vector<VkAttachmentStoreOp>& storeOp,
                                                      const std::vector<VkImageLayout>& layout,
                                                      VkPipelineBindPoint bindPoint) const {
    return std::make_shared<RenderPass>(*this, attachments, loadOp, storeOp, layout, bindPoint, "Render Pass");
}

std::shared_ptr<Pipeline>
Context::createGraphicsPipeline(const Pipeline::GraphicsPipelineDescriptor& graphicsPipelineDescriptor,
                                VkRenderPass renderPass, const std::string& name) {
    return std::make_shared<Pipeline>(this, graphicsPipelineDescriptor, renderPass, name);
}

std::shared_ptr<CommandQueueManager>
Context::createGraphicsCommandQueue(uint32_t
                                    imageCount,
                                    uint32_t concurrentNumCommands,
                                    const std::string& name,
                                    int graphicsQueueIndex
) {

    if (graphicsQueueIndex != -1) {
        ASSERT(graphicsQueueIndex < graphicsQueues_.size(), "Bad graphics queue index");
    }

    return std::make_shared<CommandQueueManager>(*this, device_,
                                                 graphicsQueueIndex != -1 ? graphicsQueues_[graphicsQueueIndex]
                                                                          : graphicsQueues_[0],
                                                 physicalDevice_.getGraphicsFamilyIndex().value(),
                                                 imageCount, concurrentNumCommands);
}

std::unique_ptr<Framebuffer>
Context::createFramebuffer(VkRenderPass renderPass, std::vector<std::shared_ptr<Texture>> colorAttachments,
                           std::shared_ptr<Texture> depthAttachment, std::shared_ptr<Texture> stencilAttachment,
                           const std::string& name) const {
    return std::make_unique<Framebuffer>(*this, device_, renderPass, colorAttachments, depthAttachment,
                                         stencilAttachment, name);
}


std::shared_ptr<Buffer> Context::createBuffer(size_t size,
                      VkBufferUsageFlags flags, VmaMemoryUsage memUsage,
                      const std::string& name
) const {
    return std::make_shared<Buffer>(
        this,
        allocator(),
        VkBufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = flags
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            .usage = memUsage,
            .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT
        },
        name

    );
}


std::shared_ptr<Buffer> Context::createPersistentBuffer(size_t size, VkBufferUsageFlags usage, const std::string& name) const {

    const VkMemoryPropertyFlags cpuVisibleMemoryFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // |
        // VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    const VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    const VmaAllocationCreateInfo allocationCreateInfo = {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        .requiredFlags = cpuVisibleMemoryFlags
    };

    return std::make_shared<Buffer>(this, allocator(), bufferCreateInfo, allocationCreateInfo, name);
}

// CPU memory mapped to GPU memory, will make transferring data quick
std::shared_ptr<Buffer> Context::createStagingBuffer(VkDeviceSize size, const std::string& name) const {

    const VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };

    const VmaAllocationCreateInfo allocationCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        // ^ requests mapping (sequentially, so no caching necessary) |  memory will be persistently mapped, and we will have a pointer to it (VmaAllocationInfo::pMappedData)
        .usage = VMA_MEMORY_USAGE_CPU_ONLY,
    };

    return std::make_shared<Buffer>(this, allocator(), bufferCreateInfo, allocationCreateInfo, name);
}

std::shared_ptr<Buffer> Context::createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage, Buffer* targetBuffer,
                                                     const std::string& name) const {
    return std::make_shared<Buffer>(this, allocator(), size, usage, targetBuffer, name);
}

void Context::uploadToGPUBuffer(CommandQueueManager& queueMgr, VkCommandBuffer commandBuffer, Buffer* gpuBuffer,
                                const void* data, long totalSize, uint64_t gpuBufferOffset) const {
    auto stagingBuffer = createStagingBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, gpuBuffer, "staging buffer");
    stagingBuffer->copyDataToBuffer(data, totalSize);

    stagingBuffer->uploadStagingBufferToGPU(commandBuffer, 0, gpuBufferOffset);
    queueMgr.disposeWhenSubmitCompletes(std::move(stagingBuffer));
}

std::shared_ptr<Sampler>
Context::createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
                       VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW, float maxLod,
                       const std::string& name) const {
    return std::make_shared<Sampler>(*this, minFilter, magFilter, addressModeU, addressModeV, addressModeW, maxLod,
                                     name);
}

std::shared_ptr<Sampler>
Context::createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
                       VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW, float maxLod,
                       bool compareEnable, VkCompareOp compareOp, const std::string& name) const {
    return std::make_shared<Sampler>(*this, minFilter, magFilter, addressModeU, addressModeV, addressModeW, maxLod,
                                     compareEnable, compareOp, name);
}

std::shared_ptr<Texture>
Context::createTexture(VkImageType type, VkFormat format, VkImageCreateFlags flags, VkImageUsageFlags usageFlags,
                       VkExtent3D extents, uint32_t numMipLevels, uint32_t layerCount,
                       VkMemoryPropertyFlags memoryFlags, bool generateMips, VkSampleCountFlagBits msaaSamples,
                       const std::string& name) const {
    return std::make_shared<Texture>(*this, type, format, flags, usageFlags, extents, numMipLevels, layerCount,
                                     memoryFlags, generateMips, msaaSamples, name);
}

void
Context::beginDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name, const glm::vec4& color) const {
#if defined(VK_EXT_debug_utils) && defined(_WIN32)
    VkDebugUtilsLabelEXT utilsLabelInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = name.c_str()
    };
    memcpy(utilsLabelInfo.color, &color[0], sizeof(float) * 4);
    vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &utilsLabelInfo);
#endif
}

void Context::endDebugUtilsLabel(VkCommandBuffer commandBuffer) const {
#if defined(VK_EXT_debug_utils) && defined(_WIN32)
    vkCmdEndDebugUtilsLabelEXT(commandBuffer);
#endif
}

void Context::createVMAAllocator() {
    // Create VMA memory allocator
    const VmaVulkanFunctions vulkanFunctions = {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = vkAllocateMemory,
        .vkFreeMemory = vkFreeMemory,
        .vkMapMemory = vkMapMemory,
        .vkUnmapMemory = vkUnmapMemory,
        .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = vkBindBufferMemory,
        .vkBindImageMemory = vkBindImageMemory,
        .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        .vkCreateBuffer = vkCreateBuffer,
        .vkDestroyBuffer = vkDestroyBuffer,
        .vkCreateImage = vkCreateImage,
        .vkDestroyImage = vkDestroyImage,
        .vkCmdCopyBuffer = vkCmdCopyBuffer,
#if VMA_VULKAN_VERSION >= 1001000
        .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
        .vkBindBufferMemory2KHR = vkBindBufferMemory2,
        .vkBindImageMemory2KHR = vkBindImageMemory2,
        .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
#endif
#if VMA_VULKAN_VERSION >= 1003000
        .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements
#endif
    };

    const VmaAllocatorCreateInfo vmaAllocatorCreateInfo = {
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
//        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT, // replay not supported on my Radeon 580
#endif
        .physicalDevice = physicalDevice_.getPhysicalDevice(),
        .device = device_,
        .pVulkanFunctions = &vulkanFunctions,
        .instance = instance_,
        .vulkanApiVersion = apiVersion
    };

    vmaCreateAllocator(&vmaAllocatorCreateInfo, &vmaAllocator_);
    std::cout << "VMA Allocator Created" << std::endl;
}


}