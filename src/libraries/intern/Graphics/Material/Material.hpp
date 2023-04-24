#pragma once

#include <intern/Datastructures/Pool.hpp>
#include <intern/Graphics/Renderer/VulkanRenderer.hpp>
#include <intern/Misc/StringHash.hpp>
#include <string_view>
#include <type_traits>
#include <unordered_map>
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

    // todo: heavy WIP, just want something functional now
    bool hasMaterialParameters();
    void setResource(std::string_view name, uint32_t index);
    void pushParameterChanges();
    Handle<Buffer> getMaterialParametersBuffer();

  private:
    void createPipeline();

    // Material Parameters -------
    uint32_t parametersBufferSize;
    // swap to "safer" storage once moving is implemented for pools?
    char* parametersBufferCPU = nullptr;
    struct ParameterInfo
    {
        // todo: use some bits to store type information etc. so I can do at least *some* tests
        uint16_t byteSize = 0xFFFF;
        uint16_t byteOffsetInBuffer = 0xFFFF;
    };
    // pointer here, again because real moving isnt implemented yet for pools
    std::unordered_map<std::string, ParameterInfo, StringHash, std::equal_to<>>* parametersLUT = nullptr;

    uint8_t currentBufferIndex = 0;
    Handle<Buffer> parametersBuffers[VulkanRenderer::FRAMES_IN_FLIGHT];

    friend ResourceManager;
};