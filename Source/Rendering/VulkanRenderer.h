#pragma once

#include <optional>

#include "vulkan/vulkan.h"

#include "VulkanWindow.h"
#include "VulkanSwapChain.h"
#include "../Utils/Common.h"
#include "../Utils/VulkanValidationLayers.h"
#include "VulkanShader.h"
#include "VulkanRenderingTypes.h"
#include "MeshBufferManager.h"
#include "../Components/Mesh/MeshComponent.h"
#include "../Components/Transform/TransformComponent.h"

namespace JoeEngine {
    class JESceneManager;
    class JEEngineInstance;
    class JEIOHandler;

    // Class that manages all Vulkan resources and rendering

    class JEVulkanRenderer {
    private:
        // Wrapper for GLFW window
        JEVulkanWindow m_vulkanWindow;

        // Wrapper for Validation Layers
        JEVulkanValidationLayers m_vulkanValidationLayers;

        // Application width and height
        uint32_t m_width;
        uint32_t m_height;

        // References to other systems
        JEEngineInstance* m_engineInstance;
        JESceneManager* m_sceneManager;

        // Other backend managers
        // JEShaderManager m_shaderManager; // TODO: create me
        JEMeshBufferManager m_meshBufferManager;

        // Vulkan Instance creation
        VkInstance m_instance;

        // Vulkan physical/logical devices
        // TODO: wrap me in VulkanDevice class (and protect with mutex)
        VkPhysicalDevice m_physicalDevice;
        VkDevice m_device;

        // Vulkan Queue(s)
        JEVulkanQueue m_graphicsQueue;
        JEVulkanQueue m_presentationQueue;

        // Swap chain
        JEVulkanSwapChain m_vulkanSwapChain;
        bool m_didFramebufferResize;

        // Framebuffers
        std::vector<VkFramebuffer> m_swapChainFramebuffers;

        // Command pool(s) and buffer(s)
        VkCommandPool m_commandPool;
        std::vector<VkCommandBuffer> m_commandBuffers;

        // Semaphores and Fences
        uint32_t m_currentFrame;
        const int m_MAX_FRAMES_IN_FLIGHT;
        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence> m_inFlightFences;

        // Setup functions
        void CreateVulkanInstance();
        std::vector<const char*> GetRequiredExtensions();
        int RateDeviceSuitability(VkPhysicalDevice physicalDevice, const JEVulkanWindow& vulkanWindow);
        void PickPhysicalDevice();
        void CreateLogicalDevice();
        void CreateCommandPool();
        void CreateSwapChainFramebuffers();
        void CreateSemaphoresAndFences();

        // Window-dependent rendering resource recreation
        void CleanupWindowDependentResources();
        void RecreateWindowDependentResources();

        /// Rendering variables and functions

        // Shaders
        // TODO: Move this to a shader manager
        std::vector<JEVulkanShadowPassShader> m_shadowPassShaders;
        JEVulkanDeferredPassGeometryShader m_deferredPassGeometryShader;
        JEVulkanDeferredPassLightingShader m_deferredPassLightingShader; // TODO: change me to a list?
        //std::vector<JEVulkanMeshShader> m_meshShaders; // TODO: add this to VulkanShader.h and re-implement
        std::vector<JEVulkanPostProcessShader> m_postProcessingShaders;
        JEVulkanFlatShader m_flatShader;
        JEVulkanForwardShader m_forwardShader;
        void CreateShaders();
        void CleanupShaders();

        // Textures
        std::vector<JETexture> m_textures;
        void CreateTextures();
        void CleanupTextures();

        // Helpers for offscreen rendering
        void CreateFramebufferAttachment(JEFramebufferAttachment& depth, VkExtent2D extent, VkImageUsageFlagBits usageBits, VkFormat format);
        void CreateFramebufferAttachmentSampler(VkSampler& sampler);

        // Shadow pass
        JEOffscreenShadowPass m_shadowPass;
        void CreateShadowPassResources();
        void CreateShadowRenderPass();
        void CreateShadowFramebuffer();
        void CreateShadowCommandBuffer();

        // Forward Rendering
        JEForwardPass m_forwardPass;
        void CreateForwardPassResources();
        void CreateForwardPassRenderPass();
        void CreateForwardPassFramebuffer();
        void CreateForwardPassCommandBuffer();

        // Deferred Rendering - geometry pass
        JEOffscreenDeferredPass m_deferredPass;
        void CreateDeferredPassGeometryResources();
        void CreateDeferredPassGeometryRenderPass();
        void CreateDeferredPassGeometryFramebuffer();
        void CreateDeferredPassGeometryCommandBuffer();

        // Deferred Rendering - lighting pass (only render offscreen if there is at least one post process)
        JEFramebufferAttachment m_framebufferAttachment_deferredLighting;
        VkRenderPass m_renderPass_deferredLighting;
        VkFramebuffer m_framebuffer_deferredLighting;
        void CreateDeferredPassLightingResources();
        void CreateDeferredPassLightingRenderPass();
        void CreateDeferredPassLightingFramebuffer();

        // Post processing
        // The final post processing pass's framebuffer attachment is never created (use the swap chain framebuffers instead)
        std::vector<JEPostProcessingPass> m_postProcessingPasses;
        void CreatePostProcessingPassResources();
        void CreatePostProcessingPassRenderPass(uint32_t i);
        void CreatePostProcessingPassFramebuffer(uint32_t i);

        void CreateDeferredLightingAndPostProcessingCommandBuffer();

        void DrawMesh(VkCommandBuffer commandBuffer, const MeshComponent& meshComponent);
        void DrawScreenSpaceTriMesh(VkCommandBuffer commandBuffer);
        void DrawBoundingBoxMesh(VkCommandBuffer commandBuffer);

        void UpdateShaderUniformBuffers(uint32_t imageIndex);

    public:
        JEVulkanRenderer() : m_width(JE_DEFAULT_SCREEN_WIDTH), m_height(JE_DEFAULT_SCREEN_HEIGHT), m_MAX_FRAMES_IN_FLIGHT(JE_DEFAULT_MAX_FRAMES_IN_FLIGHT),
                             m_currentFrame(0), m_didFramebufferResize(false), m_engineInstance(nullptr), m_sceneManager(nullptr) {}
        ~JEVulkanRenderer() {}

        // Vulkan setup
        void Initialize(JESceneManager* sceneManager, JEEngineInstance* engineInstance);
        void RegisterCallbacks(JEIOHandler* ioHandler);

        // Vulkan cleanup
        void Cleanup();

        void FramebufferResized() { m_didFramebufferResize = true; }

        // Submit work to GPU
        void SubmitFrame();

        // Mesh Buffer Manager Functions
        const std::vector<BoundingBoxData>& GetBoundingBoxData() const;
        MeshComponent CreateMesh(const std::string& filepath);

        // Renderer Functions
        void DrawShadowPass(const std::vector<MeshComponent>& meshComponents, const std::vector<TransformComponent>& transformComponents,
                            uint32_t numEntities, const JECamera& camera);
        void DrawMeshComponents(const std::vector<MeshComponent>& meshComponents, const std::vector<TransformComponent>& transformComponents, const JECamera& camera);

        void WaitForIdleDevice() {
            vkDeviceWaitIdle(m_device);
        }

        // Getters
        const JEVulkanWindow& GetWindow() const {
            return m_vulkanWindow;
        }
        GLFWwindow* GetGLFWWindow() const {
            return m_vulkanWindow.GetWindow();
        }
        VkDevice GetDevice() const {
            return m_device;
        }
    };

    static void JEFramebufferResizeCallback(GLFWwindow* window, int width, int height);
}
