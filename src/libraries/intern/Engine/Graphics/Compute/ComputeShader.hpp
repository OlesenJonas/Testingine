#pragma once

#include <vulkan/vulkan_core.h>

class ResourceManager;

struct ComputeShader
{
    VkShaderModule shaderModule = VK_NULL_HANDLE;

    VkPipeline pipeline = VK_NULL_HANDLE;

  private:
    void createPipeline();

    friend ResourceManager;
};