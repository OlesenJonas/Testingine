#pragma once

#include "../Buffer/Buffer.hpp"
#include "../Graphics.hpp"
#include <Datastructures/Pool/Handle.hpp>
#include <Datastructures/Pool/PoolHelpers.hpp>
#include <Datastructures/StringMap.hpp>
#include <Engine/Graphics/Shaders/Shaders.hpp>

// TODO: find abstraction for VkPipeline and remove include
#include <vulkan/vulkan_core.h>

namespace Material
{
    struct CreateInfo
    {
        Shaders::StageCreateInfo vertexShader;
        Shaders::StageCreateInfo fragmentShader;

        std::string_view debugName;
    };

    struct ParameterBuffer
    {
        uint32_t size = 0;
        uint8_t* writeBuffer = nullptr;
        Buffer::Handle deviceBuffer = Buffer::Handle::Invalid();
    };

    struct ParameterInfo
    {
        // todo: use some bits to store type information etc. so I can do at least *some* tests
        uint16_t byteSize = 0;
        uint16_t byteOffsetInBuffer = 0;
    };

    // TODO: need to wrap because otherwise TypeIndexInPack cant decide which index to return if pack contains
    // duplicate type
    //       TODO: could enable/allow accessing Pool through handle and index instead!
    struct ParameterMap
    {
        size_t bufferSize = 0;
        StringMap<ParameterInfo> map;
    };
    struct InstanceParameterMap
    {
        size_t bufferSize = 0;
        StringMap<ParameterInfo> map;
    };

    using Handle = Handle<std::string, VkPipeline, ParameterMap, InstanceParameterMap, ParameterBuffer, bool>;

    void setResource(Handle handle, std::string_view name, ResourceIndex index);

}; // namespace Material

namespace MaterialInstance
{
    using ParameterBuffer = Material::ParameterBuffer;
    using Handle = Handle<std::string, Material::Handle, ParameterBuffer, bool>;

    void setResource(Handle handle, std::string_view name, ResourceIndex index);
}; // namespace MaterialInstance