#pragma once
#include <Engine/Misc/EnumHelpers.hpp>
#include <cstdint>
#include <type_traits>

// see Barriers/Barrier.hpp

/*
    Not sure if I want/need these to be flags, can I get away without combining them?

    Some of these are rather braod
        IE: _graphics map to accesses in vertex OR fragment shader
            so its currently not possible to transitions something between vertex and fragment shader
            but im not even sure if ill need that. Worry about it when it happens
*/
enum struct ResourceState : uint32_t
{
    // Common states
    // doesnt map to API values, just here so uninitialized values can be caught more easily
    None = nthBit(0u),
    Undefined = nthBit(1u),
    TransferSrc = nthBit(2u),
    TransferDst = nthBit(3u),

    // TODO: Split storage into ReadOnly/WriteOnly/ReadWrite
    //       Would allow for more specific access masks and its not an uncommon use-case
    Storage = nthBit(4u),
    StorageGraphics = nthBit(5u),
    StorageCompute = nthBit(6u),

    // Texture specific
    SampleSource = nthBit(7u),
    SampleSourceGraphics = nthBit(8u),
    SampleSourceCompute = nthBit(9u),

    Rendertarget = nthBit(10u),
    DepthStencilTarget = nthBit(11u),
    DepthStencilReadOnly = nthBit(12u),

    // Buffer specific
    VertexBuffer = nthBit(13u),
    IndexBuffer = nthBit(14u),
    UniformBuffer = nthBit(15u),
    UniformBufferGraphics = nthBit(16u),
    UniformBufferCompute = nthBit(17u),
    IndirectArgument = nthBit(18u),
};

struct ResourceStateMulti
{
    using U = std::underlying_type_t<ResourceState>;

    U value = U{0};

    // explicitely want implicit conversion
    inline ResourceStateMulti(ResourceState state) : value(static_cast<U>(state)){}; // NOLINT

    inline ResourceStateMulti& operator|=(ResourceState state)
    {
        value |= static_cast<U>(state);
        return *this;
    }

    inline operator bool() const // NOLINT
    {
        return value != 0u;
    }
};

inline ResourceStateMulti operator|(ResourceStateMulti lhs, ResourceState rhs)
{
    lhs |= rhs;
    return lhs;
}

inline ResourceStateMulti operator&(ResourceStateMulti lhs, ResourceState rhs)
{
    lhs.value &= static_cast<ResourceStateMulti::U>(rhs);
    return lhs;
}

inline ResourceStateMulti operator|(ResourceState lhs, ResourceState rhs)
{
    ResourceStateMulti lhsM = lhs;
    return lhsM | rhs;
}

// TODO: move into vulkan specific folder!

#include <cassert>
#include <vulkan/vulkan_core.h>

constexpr VkImageUsageFlags toVkImageUsageSingle(ResourceState state)
{
    switch(state)
    {
    case ResourceState::None:
    case ResourceState::Undefined:
        assert(false);
    case ResourceState::TransferSrc:
        return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    case ResourceState::TransferDst:
        return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    case ResourceState::Storage:
    case ResourceState::StorageGraphics:
    case ResourceState::StorageCompute:
        return VK_IMAGE_USAGE_STORAGE_BIT;
    case ResourceState::SampleSource:
    case ResourceState::SampleSourceGraphics:
    case ResourceState::SampleSourceCompute:
        return VK_IMAGE_USAGE_SAMPLED_BIT;
    case ResourceState::Rendertarget:
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    case ResourceState::DepthStencilTarget:
    case ResourceState::DepthStencilReadOnly:
        return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    case ResourceState::VertexBuffer:
    case ResourceState::IndexBuffer:
    case ResourceState::UniformBuffer:
    case ResourceState::UniformBufferGraphics:
    case ResourceState::UniformBufferCompute:
    case ResourceState::IndirectArgument:
        assert(false);
    }
    return 0;
}

VkImageUsageFlags toVkImageUsage(ResourceStateMulti states);

constexpr VkPipelineStageFlags2 toVkPipelineStage(ResourceState state)
{
    switch(state)
    {
    case ResourceState::None:
        assert(false);
    case ResourceState::Undefined:
        // not sure how to best handle this
        return VK_PIPELINE_STAGE_2_NONE;
    case ResourceState::TransferSrc:
    case ResourceState::TransferDst:
        // TODO: could be more specific, instead of just transfer also have blit/copy_dst/src
        return VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    case ResourceState::Storage:
    case ResourceState::SampleSource:
    case ResourceState::UniformBuffer:
        return VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    case ResourceState::StorageGraphics:
    case ResourceState::SampleSourceGraphics:
    case ResourceState::UniformBufferGraphics:
        return VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    case ResourceState::StorageCompute:
    case ResourceState::SampleSourceCompute:
    case ResourceState::UniformBufferCompute:
        return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    case ResourceState::Rendertarget:
        return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    case ResourceState::DepthStencilTarget:
    case ResourceState::DepthStencilReadOnly:
        return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    case ResourceState::VertexBuffer:
        return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
    case ResourceState::IndexBuffer:
        return VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    case ResourceState::IndirectArgument:
        return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    default:
        assert(false && "Unhandled state to convert");
        return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }
}

constexpr VkAccessFlags2 toVkAccessFlags(ResourceState state)
{
    switch(state)
    {
    case ResourceState::None:
        assert(false);
        return VK_ACCESS_2_NONE;
    case ResourceState::Undefined:
        return VK_ACCESS_2_NONE;
    case ResourceState::TransferSrc:
        return VK_ACCESS_2_TRANSFER_READ_BIT;
    case ResourceState::TransferDst:
        return VK_ACCESS_2_TRANSFER_WRITE_BIT;
    case ResourceState::Storage:
    case ResourceState::StorageGraphics:
    case ResourceState::StorageCompute:
        return VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    case ResourceState::SampleSource:
    case ResourceState::SampleSourceGraphics:
    case ResourceState::SampleSourceCompute:
        return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    case ResourceState::Rendertarget:
        return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    case ResourceState::DepthStencilTarget:
        return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case ResourceState::DepthStencilReadOnly:
        return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    case ResourceState::VertexBuffer:
        return VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    case ResourceState::IndexBuffer:
        return VK_ACCESS_2_INDEX_READ_BIT;
    case ResourceState::UniformBuffer:
    case ResourceState::UniformBufferGraphics:
    case ResourceState::UniformBufferCompute:
        return VK_ACCESS_2_UNIFORM_READ_BIT;
    case ResourceState::IndirectArgument:
        return VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        break;
    }
}