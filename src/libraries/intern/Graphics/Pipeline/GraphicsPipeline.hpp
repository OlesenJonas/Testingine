#pragma once

#include <string_view>
#include <vulkan/vulkan_core.h>

struct GraphicsPipeline
{
    struct CreateInfo
    {
        struct ShaderStage
        {
            std::string_view sourcePath;
        };
        ShaderStage vertexShader;
        ShaderStage fragmentShader;
    };

    VkShaderModule vertexShader = VK_NULL_HANDLE;
    VkShaderModule fragmentShader = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayouts[4] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};