//
// Created by darby on 5/30/2024.
//

#pragma once

#include <vector>
#include <string>
#include <optional>
#include <unordered_set>
#include <set>
#include "volk.h"

namespace Fairy::Graphics {

class PhysicalDevice {

public:

    explicit PhysicalDevice() {};

    explicit PhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<std::string> requestedExtensions,
                            bool printEnums);

    void reserveQueues(VkQueueFlags requestedQueueTypes);

    [[nodiscard]] std::set<std::pair<uint32_t, uint32_t>> getReservedFamiliesAndQueueCounts() const;

    [[nodiscard]] const std::vector<std::string>& getAvailableExtensions() const;

    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const;

    [[nodiscard]] std::optional<uint32_t> getGraphicsFamilyIndex() const;

    [[nodiscard]] std::optional<uint32_t> getComputeFamilyIndex() const;

    [[nodiscard]] std::optional<uint32_t> getPresentationFamilyIndex() const;

    [[nodiscard]] std::optional<uint32_t> getTransferFamilyIndex() const;

    [[nodiscard]] std::optional<uint32_t> getSparseFamilyIndex() const;

    [[nodiscard]] uint32_t getGraphicsQueueCount() const;

    [[nodiscard]] uint32_t getPresentationQueueCount() const;

    [[nodiscard]] uint32_t getComputeQueueCount() const;

    [[nodiscard]] uint32_t getTransferQueueCount() const;

    [[nodiscard]] uint32_t getSparseQueueCount() const;

    [[nodiscard]] std::vector<VkSurfaceFormatKHR> surfaceFormats() const;

    [[nodiscard]] VkSurfaceCapabilitiesKHR surfaceCapabilities() const;

    [[nodiscard]] const std::vector<VkPresentModeKHR>& presentModes() const;

private:

    VkPhysicalDevice vkPhysicalDevice_;
    VkSurfaceKHR surface_;

    std::vector<std::string> availableExtensionsVector_;
    std::unordered_set<std::string> enabledExtensionsMap_;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties_;

    bool printEnums_;

    std::optional<uint32_t> graphicsFamilyIndex_;
    uint32_t graphicsQueueCount_ = 0;

    std::optional<uint32_t> presentationFamilyIndex_;
    uint32_t presentationQueueCount_ = 0;

    std::optional<uint32_t> computeFamilyIndex_;
    uint32_t computeQueueCount_ = 0;

    std::optional<uint32_t> transferFamilyIndex_;
    uint32_t transferQueueCount_ = 0;

    std::optional<uint32_t> sparseFamilyIndex_;
    uint32_t sparseQueueCount_ = 0;

    void enumerateSurfaceFormats(VkSurfaceKHR surface);

    void enumerateSurfaceCapabilities(VkSurfaceKHR surface);

    void enumeratePresentModes(VkSurfaceKHR surface);

    VkPhysicalDeviceFeatures physicalDeviceFeatures_;
    VkPhysicalDeviceProperties2 physicalDeviceProperties_;
    VkPhysicalDeviceMemoryProperties2 physicalDeviceMemoryProperties_;

    std::vector<VkSurfaceFormatKHR> surfaceFormats_;
    VkSurfaceCapabilitiesKHR surfaceCapabilities_;
    std::vector<VkPresentModeKHR> presentModes_;

};

}