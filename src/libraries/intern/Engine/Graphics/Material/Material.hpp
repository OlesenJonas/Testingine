#pragma once

#include "../Buffer/Buffer.hpp"
#include "../Graphics.hpp"
#include "../Texture/Texture.hpp"
#include <Datastructures/Pool/Handle.hpp>
#include <Datastructures/Pool/PoolHelpers.hpp>
#include <Datastructures/StringMap.hpp>
#include <Engine/Graphics/Shaders/Shaders.hpp>
#include <glm/glm.hpp>

// TODO: find abstraction for VkPipeline and remove include
#include <vulkan/vulkan_core.h>

namespace Material
{
    struct CreateInfo
    {
        std::string_view debugName;
        Shaders::StageCreateInfo vertexShader;
        Shaders::StageCreateInfo taskShader;
        Shaders::StageCreateInfo meshShader;
        Shaders::StageCreateInfo fragmentShader;

        // vertex input
        // --

        // fragment output
        // When not creating this struct "inside a parameter" (which guarantees lifetime until function exits)
        // the data this Span points to needs to be declared outside of this struct to ensure its lifetime!!
        Span<const Texture::Format> colorFormats;
        Texture::Format depthFormat = Texture::Format::UNDEFINED;
        Texture::Format stencilFormat = Texture::Format::UNDEFINED;
    };

    struct ParameterBuffer
    {
        uint32_t size = 0;
        uint8_t* cpuBuffer = nullptr;
        Buffer::Handle deviceBuffer = Buffer::Handle::Invalid();
    };

    struct ParameterInfo
    {
        // todo: use some bits to store type information etc. so I can do at least *some* tests
        uint16_t byteSize = 0;
        uint16_t byteOffsetInBuffer = 0;
        bool operator==(const ParameterInfo&) const = default;
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

    // like create info but only things relevant for reloading
    // and it contains owning members instead of span
    struct ReloadInfo
    {
        std::string vertexSource;
        std::string fragmentSource;
        std::vector<Texture::Format> colorFormats;
        Texture::Format depthFormat = Texture::Format::UNDEFINED;
        Texture::Format stencilFormat = Texture::Format::UNDEFINED;
    };

    using Handle =
        Handle<std::string, VkPipeline, ParameterMap, InstanceParameterMap, ParameterBuffer, bool, ReloadInfo>;

    void setResource(Handle handle, std::string_view name, ResourceIndex index);
    void setFloat(Handle handle, std::string_view name, float value);

}; // namespace Material

namespace MaterialInstance
{
    using ParameterBuffer = Material::ParameterBuffer;
    using Handle = Handle<std::string, Material::Handle, ParameterBuffer, bool>;

    void setResource(Handle handle, std::string_view name, ResourceIndex index);
    template <typename T>
    void setValue(Handle handle, std::string_view name, T value);
}; // namespace MaterialInstance