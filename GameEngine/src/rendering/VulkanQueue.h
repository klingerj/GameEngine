#pragma once

#include "vulkan/vulkan.h"

#include <optional>
#include <set>
#include <vector>

class VulkanWindow;

// Queue Family Indices
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool IsComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
std::vector<VkDeviceQueueCreateInfo> GetQueueCreateInfos(const QueueFamilyIndices& indices);

class VulkanQueue {
private:
    VkQueue queue;

public:
    VulkanQueue() : queue(VK_NULL_HANDLE) {}
    ~VulkanQueue() {}

    // Setup
    void GetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex);

    // Getters
    VkQueue GetQueue() const {
        return queue;
    }
};
