#pragma once

#include <optional>

#include "vulkan/vulkan.h"

#include "VulkanShader.h"
#include "../VulkanValidationLayers.h"
#include "../GlobalInfo.h"

// Class that manages all Vulkan resources and rendering

class VulkanRenderer {
private:
    // Wrapper for GLFW window
    VulkanWindow vulkanWindow;

    // Wrapper for Validation Layers
    VulkanValidationLayers vulkanValidationLayers;

    // Application width and height
    const uint32_t width;
    const uint32_t height;

    // Vulkan Instance creation
    VkInstance instance;

    // Vulkan physical/logical devices
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    // Vulkan Queue(s)
    VulkanQueue graphicsQueue;
    VulkanQueue presentationQueue;

    // Swap chain
    VulkanSwapChain vulkanSwapChain;

    // Shaders and rendering
    std::vector<VulkanShader> shaders;
    VkRenderPass renderPass;
    
    // Framebuffers
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Command pool(s) and buffer(s)
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // Semaphores and Fences
    size_t currentFrame;
    const int MAX_FRAMES_IN_FLIGHT;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

public:
    VulkanRenderer() : width(DEFAULT_SCREEN_WIDTH), height(DEFAULT_SCREEN_HEIGHT), MAX_FRAMES_IN_FLIGHT(DEFAULT_MAX_FRAMES_IN_FLIGHT), currentFrame(0) {}
    ~VulkanRenderer() {}

    // Vulkan setup
    void Initialize();

    // Vulkan cleanup
    void Cleanup();

    // Draw a frame
    void DrawFrame();

    // Setup functions
    void CreateVulkanInstance();
    std::vector<const char*> GetRequiredExtensions();
    int RateDeviceSuitability(const VkPhysicalDevice& physDevice, const VulkanWindow& vulkanWindow);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateRenderPass(const VulkanSwapChain& swapChain);
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSemaphoresAndFences();

    // Getters
    const VulkanWindow& GetWindow() const {
        return vulkanWindow;
    }
    const VkDevice& GetDevice() const {
        return device;
    }
};
