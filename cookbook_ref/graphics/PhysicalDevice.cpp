//
// Created by darby on 5/30/2024.
//

#include <algorithm>
#include "PhysicalDevice.hpp"

#include "VulkanUtils.hpp"
#include "CoreUtils.hpp"

namespace Fairy::Graphics {

std::vector<std::string> queryDeviceExtensions(VkPhysicalDevice device) {

    uint32_t extensionCount{0};
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr));

    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensionProperties.data()));

    std::vector<std::string> extensionPropertyStrings;
    std::transform(
        extensionProperties.begin(), extensionProperties.end(),
        std::back_inserter(extensionPropertyStrings),
        [](const VkExtensionProperties& properties) {
            return std::string(properties.extensionName);
        });

    return extensionPropertyStrings;
}

PhysicalDevice::PhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,
                               std::vector<std::string> requestedExtensions, bool printEnums) : vkPhysicalDevice_(
    device), surface_(surface), printEnums_(printEnums) {

    availableExtensionsVector_ = queryDeviceExtensions(vkPhysicalDevice_);

    enabledExtensionsMap_ = Fairy::Graphics::filterExtensions(availableExtensionsVector_, requestedExtensions);

    // Get Features
    vkGetPhysicalDeviceFeatures(vkPhysicalDevice_, &physicalDeviceFeatures_);

    // Get Properties
    physicalDeviceProperties_ = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };
    vkGetPhysicalDeviceProperties2(vkPhysicalDevice_, &physicalDeviceProperties_);

    // Get Memory Properties
    physicalDeviceMemoryProperties_ = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        .pNext = nullptr
    };
    vkGetPhysicalDeviceMemoryProperties2(vkPhysicalDevice_, &physicalDeviceMemoryProperties_);

    // Get Family Queue Properties
    uint32_t queueFamilyCount{0};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    queueFamilyProperties_.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties_.data());

    // Get data for swapchain
    if (surface != VK_NULL_HANDLE) {
        enumeratePresentModes(surface);
        enumerateSurfaceCapabilities(surface);
        enumeratePresentModes(surface);
    }

    if (printEnums) {
        std::cerr << physicalDeviceProperties_.properties.deviceName << " "
                  << physicalDeviceProperties_.properties.vendorID << " ("
                  << physicalDeviceProperties_.properties.deviceID << ") - ";
        const auto apiVersion = physicalDeviceProperties_.properties.apiVersion;
        std::cerr << "Vulkan " << VK_API_VERSION_MAJOR(apiVersion) << "."
                  << VK_API_VERSION_MINOR(apiVersion) << "."
                  << VK_API_VERSION_PATCH(apiVersion) << "."
                  << VK_API_VERSION_VARIANT(apiVersion) << ")" << std::endl;

        std::cerr << "Available Extensions: " << std::endl;
        for (const auto& extension: availableExtensionsVector_) {
            std::cerr << "\t" << extension << std::endl;
        }

        std::cerr << "Supported surface formats: " << std::endl;
        for (const auto format: surfaceFormats_) {
            std::cerr << "\t" << string_VkFormat(format.format) << " : "
                      << string_VkColorSpaceKHR(format.colorSpace) << std::endl;
            std::cerr << "\t" << format.format << " : " << format.colorSpace << std::endl;
        }

        std::cerr << "Supported presentation modes: " << std::endl;
        for (const auto mode: presentModes_) {
            std::cerr << "\t" << string_VkPresentModeKHR(mode) << std::endl;
        }
    }

}

/*
 * Will only reserve presentation and graphics queues currently
 */
void PhysicalDevice::reserveQueues(VkQueueFlags requestedQueueTypes) {


    for (uint32_t queueFamilyIndex = 0;
         queueFamilyIndex < queueFamilyProperties_.size() && requestedQueueTypes != 0; queueFamilyIndex++) {

        // Suggestion from Modern Vulkan Cookbook:
        // We only share queues with presentation, a vulkan queue can support multiple
        // operations (such as graphics, compute, sparse, transfer etc), however in
        // that case that queue can be used only through one thread This code ensures
        // we treat each queue as independent and it can only be used either for
        // graphics, compute, transfer or sparse, this helps us with multithreading,
        // however if the device only have one queue for all, then because of this
        // code we may not be able to create compute/transfer queues

        // Presentation Family Queue (We CAN share this queue)
        if (!presentationFamilyIndex_.has_value() && surface_ != VK_NULL_HANDLE) {
            VkBool32 supportsPresent{VK_FALSE};

            vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice_, queueFamilyIndex, surface_, &supportsPresent);

            if (supportsPresent == VK_TRUE) {
                presentationFamilyIndex_ = queueFamilyIndex;
                presentationQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            }
        }

        // Graphics Family Queue
        if (!graphicsFamilyIndex_.has_value() &&
            queueFamilyProperties_[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamilyIndex_ = queueFamilyIndex;
            graphicsQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            requestedQueueTypes &= ~VK_QUEUE_GRAPHICS_BIT;
            continue;
        }

        // Compute Family Queue
        if (!computeFamilyIndex_.has_value() &&
            queueFamilyProperties_[queueFamilyIndex].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            computeFamilyIndex_ = queueFamilyIndex;
            computeQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            requestedQueueTypes &= ~VK_QUEUE_COMPUTE_BIT;
            continue;
        }

        // Transfer Family Queue
        if (!transferFamilyIndex_.has_value() &&
            queueFamilyProperties_[queueFamilyIndex].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            transferFamilyIndex_ = queueFamilyIndex;
            transferQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            requestedQueueTypes &= ~VK_QUEUE_TRANSFER_BIT;
            continue;
        }

        // Sparse Family Queue
        if (!sparseFamilyIndex_.has_value() &&
            queueFamilyProperties_[queueFamilyIndex].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
            sparseFamilyIndex_ = queueFamilyIndex;
            sparseQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            requestedQueueTypes &= ~VK_QUEUE_SPARSE_BINDING_BIT;
            continue;
        }

        ASSERT(presentationFamilyIndex_.has_value(), "No present queues found");
    }

}

/*
 * Returns <familyIndex, queueCount>
 * We return a SET, as returning the same family queue twice does not work/make sense.
 */
std::set<std::pair<uint32_t, uint32_t>> PhysicalDevice::getReservedFamiliesAndQueueCounts() const {

    std::set<std::pair<uint32_t, uint32_t>> reservedFamiliesAndQueuesCounts;

    if (presentationFamilyIndex_.has_value()) {
        reservedFamiliesAndQueuesCounts.insert({presentationFamilyIndex_.value(), presentationQueueCount_});
    }

    if (graphicsFamilyIndex_.has_value()) {
        reservedFamiliesAndQueuesCounts.insert({graphicsFamilyIndex_.value(), graphicsQueueCount_});
    }

    if (computeFamilyIndex_.has_value()) {
        reservedFamiliesAndQueuesCounts.insert({computeFamilyIndex_.value(), computeQueueCount_});
    }

    if (transferFamilyIndex_.has_value()) {
        reservedFamiliesAndQueuesCounts.insert({transferFamilyIndex_.value(), transferQueueCount_});
    }

    if (sparseFamilyIndex_.has_value()) {
        reservedFamiliesAndQueuesCounts.insert({sparseFamilyIndex_.value(), sparseQueueCount_});
    }

    return reservedFamiliesAndQueuesCounts;
}

const std::vector<std::string>& PhysicalDevice::getAvailableExtensions() const {
    return availableExtensionsVector_;
}

VkPhysicalDevice PhysicalDevice::getPhysicalDevice() const {
    return vkPhysicalDevice_;
}

std::optional<uint32_t> PhysicalDevice::getGraphicsFamilyIndex() const {
    return graphicsFamilyIndex_;
}

std::optional<uint32_t> PhysicalDevice::getComputeFamilyIndex() const {
    return computeFamilyIndex_;
}

std::optional<uint32_t> PhysicalDevice::getPresentationFamilyIndex() const {
    return presentationFamilyIndex_;
}

std::optional<uint32_t> PhysicalDevice::getTransferFamilyIndex() const {
    return transferFamilyIndex_;
}

std::optional<uint32_t> PhysicalDevice::getSparseFamilyIndex() const {
    return sparseFamilyIndex_;
}

uint32_t PhysicalDevice::getGraphicsQueueCount() const {
    return graphicsQueueCount_;
}

uint32_t PhysicalDevice::getPresentationQueueCount() const {
    return presentationQueueCount_;
}

uint32_t PhysicalDevice::getComputeQueueCount() const {
    return computeQueueCount_;
}

uint32_t PhysicalDevice::getTransferQueueCount() const {
    return transferQueueCount_;
}

uint32_t PhysicalDevice::getSparseQueueCount() const {
    return sparseQueueCount_;
}

void PhysicalDevice::enumerateSurfaceFormats(VkSurfaceKHR surface) {
    uint32_t formatCount{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, surface, &formatCount, nullptr);
    surfaceFormats_.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, surface, &formatCount, surfaceFormats_.data());
}

void PhysicalDevice::enumerateSurfaceCapabilities(VkSurfaceKHR surface) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, surface, &surfaceCapabilities_);
}

void PhysicalDevice::enumeratePresentModes(VkSurfaceKHR surface) {
    uint32_t presentModeCount{0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, surface, &presentModeCount, nullptr);
    presentModes_.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, surface, &presentModeCount, presentModes_.data());
}

std::vector<VkSurfaceFormatKHR> PhysicalDevice::surfaceFormats() const {
    return surfaceFormats_;
}

VkSurfaceCapabilitiesKHR PhysicalDevice::surfaceCapabilities() const {
    return surfaceCapabilities_;
}

const std::vector<VkPresentModeKHR>& PhysicalDevice::presentModes() const {
    return presentModes_;
}

}