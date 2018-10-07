#pragma once

#include <fstream>
#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanSwapChain.h"

#include "Mesh.h" // TODO: make separate shader classes, this class shouldn't be limited to MeshVertex's binding/attribute description
#include "Texture.h"
#include "Camera.h"

struct UBO_MVP {
    glm::mat4 mvp;
};

// Class to wrap pipelines and descriptors

class VulkanShader {
private:
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> descriptorSets;

    // Buffer info
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
public:
    VulkanShader() {}
    VulkanShader(VkPhysicalDevice physicalDevice, VkDevice device, const VulkanSwapChain& swapChain, VkRenderPass renderPass,
                 const Texture& texture, const std::string& vertShader, const std::string& fragShader) {
        // Read in shader code
        auto vertShaderCode = ReadFile(vertShader);
        auto fragShaderCode = ReadFile(fragShader);

        // Create shader modules
        VkShaderModule vertShaderModule = CreateShaderModule(device, vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(device, fragShaderCode);

        size_t numSwapChainImages = swapChain.GetImageViews().size();
        CreateUniformBuffer(physicalDevice, device, numSwapChainImages);
        CreateDescriptorSetLayout(device);
        CreateDescriptorPool(device, numSwapChainImages);
        CreateDescriptorSets(device, texture, numSwapChainImages);

        CreateGraphicsPipeline(device, vertShaderModule, fragShaderModule, swapChain, renderPass);
    }

    ~VulkanShader() {}

    void Cleanup(VkDevice device);
    
    // Creation functions
    void CreateGraphicsPipeline(VkDevice device, VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
                                const VulkanSwapChain& swapChain, VkRenderPass renderPass);
    VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code);
    std::vector<char> VulkanShader::ReadFile(const std::string& filename) const;

    // Descriptors
    void CreateDescriptorPool(VkDevice device, size_t numSwapChainImages);
    void CreateDescriptorSetLayout(VkDevice device);
    void CreateDescriptorSets(VkDevice device, const Texture& texture, size_t numSwapChainImages);
    void CreateUniformBuffer(VkPhysicalDevice physicalDevice, VkDevice device, size_t numSwapChainImages);
    void UpdateUniformBuffer(VkDevice device, uint32_t currentImage, const Camera& camera);
    void BindDescriptorSets(VkCommandBuffer commandBuffer, size_t descriptorSetIndex);

    // Getters
    const VkPipeline& GetPipeline() {
        return graphicsPipeline;
    }
};
