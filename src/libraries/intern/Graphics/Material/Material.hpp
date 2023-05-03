#pragma once

#include <intern/Datastructures/Pool.hpp>
#include <intern/Graphics/Renderer/VulkanRenderer.hpp>
#include <intern/Misc/Concepts.hpp>
#include <intern/Misc/StringHash.hpp>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

#include "ParameterBuffer.hpp"

class ResourceManager;

struct ParameterInfo
{
    // todo: use some bits to store type information etc. so I can do at least *some* tests
    uint16_t byteSize = 0xFFFF;
    uint16_t byteOffsetInBuffer = 0xFFFF;
};

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

    ParameterBuffer parameters;

  private:
    void createPipeline();

    using ParameterMap = std::unordered_map<std::string, ParameterInfo, StringHash, std::equal_to<>>;
    // not sure I like this being a pointer, but for now it makes stuff easier
    ParameterMap* parametersLUT = nullptr;
    ParameterMap* instanceParametersLUT = nullptr;
    uint32_t parametersBufferSize = 0;
    uint32_t instanceParametersBufferSize = 0;

    friend ParameterBuffer;
    friend ResourceManager;
};

static_assert(is_trivially_relocatable<Material>);