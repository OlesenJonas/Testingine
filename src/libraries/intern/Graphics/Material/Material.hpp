#pragma once

#include <intern/Datastructures/Pool.hpp>
#include <string_view>
#include <type_traits>
#include <vulkan/vulkan_core.h>

class ResourceManager;

struct Material
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

    VkPipeline pipeline = VK_NULL_HANDLE;

  private:
    void createPipeline();

    friend ResourceManager;
};