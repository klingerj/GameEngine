#pragma once

#include <optional>

#include "vulkan/vulkan.h"

#include "VulkanWindow.h"
#include "VulkanSwapChain.h"
#include "../utils/Common.h"
#include "../utils/VulkanValidationLayers.h"

class SceneManager;

// Rendering-related structs

typedef struct framebuffer_attachment_t {
    VkImage image;
    VkDeviceMemory deviceMemory;
    VkImageView imageView;
} FramebufferAttachment;

typedef struct offscreen_shadow_pass_t {
    int32_t width = DEFAULT_SHADOW_MAP_WIDTH, height = DEFAULT_SHADOW_MAP_HEIGHT;
    VkFramebuffer framebuffer;
    FramebufferAttachment depth;
    VkRenderPass renderPass;
    VkSampler depthSampler;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE; // Semaphore used to synchronize between this and the next render pass
} OffscreenShadowPass;

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

    // Scene Manager
    SceneManager* sceneManager;

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
    bool framebufferResized;

    // Depth buffer
    FramebufferAttachment depthBuffer;

    // Renderpass(es)
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

    // Setup functions
    void CreateVulkanInstance();
    std::vector<const char*> GetRequiredExtensions();
    int RateDeviceSuitability(VkPhysicalDevice physicalDevice, const VulkanWindow& vulkanWindow);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSemaphoresAndFences();

    // Swap chain recreation
    void CleanupSwapChain();
    void RecreateSwapChain();

    // Rendering
    OffscreenShadowPass shadowPass;
    void CreateShadowPassResources();
    void CreateShadowRenderPass();
    void CreateShadowFramebuffer();
    void CreateDepthAttachment(FramebufferAttachment& depth, VkExtent2D extent, VkImageUsageFlagBits usageBits);
    void CreateDepthSampler(VkSampler& sampler);
    void CreateShadowCommandBuffer();

public:
    VulkanRenderer() : width(DEFAULT_SCREEN_WIDTH), height(DEFAULT_SCREEN_HEIGHT), MAX_FRAMES_IN_FLIGHT(DEFAULT_MAX_FRAMES_IN_FLIGHT),
                       currentFrame(0), framebufferResized(false), sceneManager(nullptr) {}
    ~VulkanRenderer() {}

    // Vulkan setup
    void Initialize(SceneManager* sceneManager);

    // Vulkan cleanup
    void Cleanup();
    
    void FramebufferResized() { framebufferResized = true; }

    // Draw a frame
    void DrawFrame();

    // Getters
    const VulkanWindow& GetWindow() const {
        return vulkanWindow;
    }
    VkDevice GetDevice() const {
        return device;
    }
};

static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
