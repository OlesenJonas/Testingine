#pragma once

#include <Datastructures/Pool/Handle.hpp>
#include <Datastructures/Pool/PoolHelpers.hpp>
#include <Datastructures/StringMap.hpp>
#include <Engine/Graphics/Shaders/Shaders.hpp>

// TODO: find abstraction for VkPipeline and remove include
#include <vulkan/vulkan_core.h>

struct Buffer;

struct ParameterBuffer
{
    uint32_t bufferSize = 0;
    Handle<Buffer> writeBuffer = Handle<Buffer>::Invalid();
    Handle<Buffer> deviceBuffer = Handle<Buffer>::Invalid();
};

struct Material
{
    struct CreateInfo
    {
        Shaders::StageCreateInfo vertexShader;
        Shaders::StageCreateInfo fragmentShader;

        std::string_view debugName;
    };

    struct ParameterInfo
    {
        // todo: use some bits to store type information etc. so I can do at least *some* tests
        uint16_t byteSize = 0;
        uint16_t byteOffsetInBuffer = 0;
    };
    using ParameterMap = StringMap<ParameterInfo>;

    std::string name;

    ParameterMap parametersLUT;
    size_t parametersBufferSize = 0;
    ParameterMap instanceParametersLUT;
    size_t instanceParametersBufferSize = 0;

    ParameterBuffer parameters;

    VkPipeline pipeline = VK_NULL_HANDLE;

    bool dirty = false;

    void setResource(std::string_view name, uint32_t index);
};

static_assert(std::is_move_constructible<Material>::value);

struct MaterialInstance
{
    Handle<Material> parentMaterial;

    ParameterBuffer parameters;

    bool dirty = false;

    void setResource(std::string_view name, uint32_t index);
};

static_assert(std::is_move_constructible<MaterialInstance>::value);
static_assert(PoolHelper::is_trivially_relocatable<MaterialInstance>);