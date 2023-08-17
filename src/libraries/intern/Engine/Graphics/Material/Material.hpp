#pragma once

#include "ParameterBuffer.hpp"
#include <Engine/Graphics/Shaders/Shaders.hpp>
#include <Engine/Misc/StringHash.hpp>

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
        Shaders::StageCreateInfo vertexShader;
        Shaders::StageCreateInfo fragmentShader;

        std::string_view debugName;
    };

    VkPipeline pipeline = VK_NULL_HANDLE;

    ParameterBuffer parameters;

  private:
    using ParameterMap = std::unordered_map<std::string, ParameterInfo, StringHash, std::equal_to<>>;
    // not sure I like this being a pointer, but for now it makes stuff easier
    ParameterMap* parametersLUT = nullptr;
    ParameterMap* instanceParametersLUT = nullptr;
    uint32_t parametersBufferSize = 0;
    uint32_t instanceParametersBufferSize = 0;

    friend ParameterBuffer;
    friend ResourceManager;
};

static_assert(PoolHelper::is_trivially_relocatable<Material>);