#pragma once

#include <fstream>
#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanSwapChain.h"
#include "VulkanRenderer.h"
#include "Texture.h"
#include "../scene/Camera.h"

struct JEUBO_ViewProj {
    glm::mat4 viewProj;
};

struct JEUBO_ViewProj_Inv {
    glm::mat4 invProj;
    glm::mat4 invView;
};

struct JEUBODynamic_ModelMat {
    glm::mat4* model = nullptr;
};

std::vector<char> ReadFile(const std::string& filename);
VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code);

// Post Processing Shader: Draws a meshes with a texture uniform.

class JEVulkanPostProcessShader {
private:
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> descriptorSets;

    // Buffer info
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    // Creation functions
    void CreateGraphicsPipeline(VkDevice device, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
        const JEVulkanSwapChain& swapChain, VkRenderPass renderPass);
    void CreateDescriptorPool(VkDevice device, size_t numSwapChainImages);
    void CreateDescriptorSetLayout(VkDevice device);
    void CreateDescriptorSets(VkDevice device, const JEPostProcessingPass& postProcessingPass, VkImageView postImageView, size_t numSwapChainImages);
    void CreateUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice device, size_t numSwapChainImages);

public:
    JEVulkanPostProcessShader() {}
    JEVulkanPostProcessShader(VkPhysicalDevice physicalDevice, VkDevice device, const JEVulkanSwapChain& swapChain, const JEPostProcessingPass& postProcessingPass,
                            VkImageView postImageView, const std::string& vertShader, const std::string& fragShader) {
        // Read in shader code
        auto vertShaderCode = ReadFile(vertShader);
        auto fragShaderCode = ReadFile(fragShader);

        // Create shader modules
        VkShaderModule vertShaderModule = CreateShaderModule(device, vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(device, fragShaderCode);

        size_t numSwapChainImages = swapChain.GetImageViews().size();
        CreateUniformBuffers(physicalDevice, device, numSwapChainImages);
        CreateDescriptorSetLayout(device);
        CreateDescriptorPool(device, numSwapChainImages);
        CreateDescriptorSets(device, postProcessingPass, postImageView, numSwapChainImages);
        CreateGraphicsPipeline(device, vertShaderModule, fragShaderModule, swapChain, postProcessingPass.renderPass);
    }

    ~JEVulkanPostProcessShader() {}

    void Cleanup(VkDevice device);

    void UpdateUniformBuffers(VkDevice device, uint32_t currentImage, const JECamera& camera, const JECamera& shadowCamera, const glm::mat4* modelMatrices, uint32_t numMeshes);
    void BindDescriptorSets(VkCommandBuffer commandBuffer, size_t descriptorSetIndex);

    // Getters
    VkPipeline GetPipeline() const {
        return graphicsPipeline;
    }
};

// Shadow pass Shadow: Render to depth from the perspective of a light source

class JEVulkanShadowPassShader {
private:
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;

    // Buffer info
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkBuffer uniformBuffers_ViewProj;
    VkDeviceMemory uniformBuffersMemory_ViewProj;
    size_t uboDynamicAlignment;
    JEUBODynamic_ModelMat ubo_Dynamic_ModelMat;
    VkBuffer uniformBuffers_Dynamic_Model;
    VkDeviceMemory uniformBuffersMemory_Dynamic_Model;

    // Creation functions
    void CreateGraphicsPipeline(VkDevice device, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule, VkExtent2D extent, VkRenderPass renderPass);
    void CreateDescriptorPool(VkDevice device);
    void CreateDescriptorSetLayout(VkDevice device);
    void CreateDescriptorSets(VkDevice device);
    void CreateUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice device, size_t numModelMatrices);

public:
    JEVulkanShadowPassShader() : uboDynamicAlignment(0), ubo_Dynamic_ModelMat() {}
    JEVulkanShadowPassShader(VkPhysicalDevice physicalDevice, VkDevice device, VkRenderPass renderPass, VkExtent2D extent, size_t numModelMatrices,
                           const std::string& vertShader, const std::string& fragShader) {
        // Read in shader code
        auto vertShaderCode = ReadFile(vertShader);
        auto fragShaderCode = ReadFile(fragShader);

        // Create shader modules
        VkShaderModule vertShaderModule = CreateShaderModule(device, vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(device, fragShaderCode);

        CreateUniformBuffers(physicalDevice, device, numModelMatrices);
        CreateDescriptorSetLayout(device);
        CreateDescriptorPool(device);
        CreateDescriptorSets(device);
        CreateGraphicsPipeline(device, vertShaderModule, fragShaderModule, extent, renderPass);
    }

    ~JEVulkanShadowPassShader() {}

    void Cleanup(VkDevice device);

    void UpdateUniformBuffers(VkDevice device, const JECamera& camera, const glm::mat4* modelMatrices, uint32_t numMeshes);
    void BindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t dynamicOffset);

    // Getters
    VkPipeline GetPipeline() const {
        return graphicsPipeline;
    }
    size_t GetDynamicAlignment() const {
        return uboDynamicAlignment;
    }
};

// Deferred Geometry Pass Shader: Renders meshes to g-buffers

class JEVulkanDeferredPassGeometryShader {
private:
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;

    // Buffer info
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkBuffer uniformBuffers_ViewProj;
    VkDeviceMemory uniformBuffersMemory_ViewProj;
    size_t uboDynamicAlignment;
    JEUBODynamic_ModelMat ubo_Dynamic_ModelMat;
    VkBuffer uniformBuffers_Dynamic_Model;
    VkDeviceMemory uniformBuffersMemory_Dynamic_Model;

    // Creation functions
    void CreateGraphicsPipeline(VkDevice device, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
        const JEVulkanSwapChain& swapChain, VkRenderPass renderPass);
    void CreateDescriptorPool(VkDevice device);
    void CreateDescriptorSetLayout(VkDevice device);
    void CreateDescriptorSets(VkDevice device, const JETexture& texture);
    void CreateUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice device, size_t numModelMatrices);

public:
    JEVulkanDeferredPassGeometryShader() : uboDynamicAlignment(0), ubo_Dynamic_ModelMat() {}
    JEVulkanDeferredPassGeometryShader(VkPhysicalDevice physicalDevice, VkDevice device, const JEVulkanSwapChain& swapChain, VkRenderPass renderPass,
        size_t numModelMatrices, const JETexture& texture, const std::string& vertShader, const std::string& fragShader) {
        // Read in shader code
        auto vertShaderCode = ReadFile(vertShader);
        auto fragShaderCode = ReadFile(fragShader);

        // Create shader modules
        VkShaderModule vertShaderModule = CreateShaderModule(device, vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(device, fragShaderCode);

        size_t numSwapChainImages = swapChain.GetImageViews().size();
        CreateUniformBuffers(physicalDevice, device, numModelMatrices);
        CreateDescriptorSetLayout(device);
        CreateDescriptorPool(device);
        CreateDescriptorSets(device, texture);
        CreateGraphicsPipeline(device, vertShaderModule, fragShaderModule, swapChain, renderPass);
    }

    ~JEVulkanDeferredPassGeometryShader() {}

    void Cleanup(VkDevice device);

    void UpdateUniformBuffers(VkDevice device, const JECamera& camera, const glm::mat4* modelMatrices, uint32_t numMeshes);
    void BindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t dynamicOffset);

    // Getters
    VkPipeline GetPipeline() const {
        return graphicsPipeline;
    }
    size_t GetDynamicAlignment() const {
        return uboDynamicAlignment;
    }
};

// Deferred Lighting Pass: Renders a scene using G-buffers

class JEVulkanDeferredPassLightingShader {
private:
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> descriptorSets;

    // Buffer info
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    std::vector<VkBuffer> uniformBuffers_ViewProj;
    std::vector<VkDeviceMemory> uniformBuffersMemory_ViewProj;
    std::vector<VkBuffer> uniformBuffers_ViewProj_Shadow;
    std::vector<VkDeviceMemory> uniformBuffersMemory_ViewProj_Shadow;

    // Creation functions
    void CreateGraphicsPipeline(VkDevice device, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
        const JEVulkanSwapChain& swapChain, VkRenderPass renderPass);
    void CreateDescriptorPool(VkDevice device, size_t numSwapChainImages);
    void CreateDescriptorSetLayout(VkDevice device);
    void CreateDescriptorSets(VkDevice device, const JETexture& texture, const JEOffscreenShadowPass& shadowPass, const JEOffscreenDeferredPass& deferredPass, size_t numSwapChainImages);
    void CreateUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice device, size_t numSwapChainImages);

public:
    JEVulkanDeferredPassLightingShader() {}
    JEVulkanDeferredPassLightingShader(VkPhysicalDevice physicalDevice, VkDevice device, const JEVulkanSwapChain& swapChain, const JEOffscreenShadowPass& shadowPass, const JEOffscreenDeferredPass& deferredPass,
        VkRenderPass renderPass, const JETexture& texture, const std::string& vertShader, const std::string& fragShader) {
        // Read in shader code
        auto vertShaderCode = ReadFile(vertShader);
        auto fragShaderCode = ReadFile(fragShader);

        // Create shader modules
        VkShaderModule vertShaderModule = CreateShaderModule(device, vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(device, fragShaderCode);

        size_t numSwapChainImages = swapChain.GetImageViews().size();
        CreateUniformBuffers(physicalDevice, device, numSwapChainImages);
        CreateDescriptorSetLayout(device);
        CreateDescriptorPool(device, numSwapChainImages);
        CreateDescriptorSets(device, texture, shadowPass, deferredPass, numSwapChainImages);
        CreateGraphicsPipeline(device, vertShaderModule, fragShaderModule, swapChain, renderPass);
    }

    ~JEVulkanDeferredPassLightingShader() {}

    void Cleanup(VkDevice device);

    void UpdateUniformBuffers(VkDevice device, uint32_t currentImage, const JECamera& camera, const JECamera& shadowCamera);
    void BindDescriptorSets(VkCommandBuffer commandBuffer, size_t descriptorSetIndex);

    // Getters
    VkPipeline GetPipeline() const {
        return graphicsPipeline;
    }
};
