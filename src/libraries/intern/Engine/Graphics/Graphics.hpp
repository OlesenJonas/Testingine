#pragma once
#include <Datastructures/Pool/Pool.hpp>
#include <Engine/Misc/EnumHelpers.hpp>
#include <cstdint>
#include <type_traits>
#include <variant>

// see Barrier/Barrier.hpp

/*
    Not sure if I want/need these to be flags, can I get away without combining them?

    Some of these are rather braod
        IE: _graphics map to accesses in vertex OR fragment shader
            so its currently not possible to transitions something between vertex and fragment shader
            but im not even sure if ill need that. Worry about it when it happens
*/

// clang-format off
enum struct ResourceState : uint32_t
{
    // Common states
    // doesnt map to API values, just here so uninitialized values can be caught more easily
    None        = nthBit(0u),

    Undefined   = nthBit(1u),
    TransferSrc = nthBit(2u),
    TransferDst = nthBit(3u),

    Storage              = nthBit(4u),
    StorageGraphics      = nthBit(5u),
    StorageCompute       = nthBit(6u),

    // Texture specific
    SampleSource         = nthBit(7u),
    SampleSourceGraphics = nthBit(8u),
    SampleSourceCompute  = nthBit(9u),

    Rendertarget         = nthBit(10u),
    DepthStencilTarget   = nthBit(11u),
    DepthStencilReadOnly = nthBit(12u),

    // Buffer specific
    VertexBuffer          = nthBit(13u),
    IndexBuffer           = nthBit(14u),
    UniformBuffer         = nthBit(15u),
    UniformBufferGraphics = nthBit(16u),
    UniformBufferCompute  = nthBit(17u),
    IndirectArgument      = nthBit(18u),

    // Swapchain specific
    OldSwapchainImage     = nthBit(19u),
    PresentSrc            = nthBit(20u),
};
// clang-format on

struct ResourceStateMulti
{
    using U = std::underlying_type_t<ResourceState>;

    U value = U{0};

    // explicitely want implicit conversion
    constexpr inline ResourceStateMulti(ResourceState state) : value(static_cast<U>(state)){}; // NOLINT

    constexpr inline ResourceStateMulti& operator|=(ResourceState state)
    {
        value |= static_cast<U>(state);
        return *this;
    }

    constexpr inline explicit operator bool() const { return value != 0u; }

    constexpr void unset(const ResourceStateMulti& rhs) { value &= ~static_cast<U>(rhs.value); }
    bool containsUniformBufferUsage();
    bool containsStorageBufferUsage();
};

constexpr inline ResourceStateMulti operator~(ResourceStateMulti lhs)
{
    lhs.value = ~lhs.value;
    return lhs;
}

constexpr inline ResourceStateMulti operator|(ResourceStateMulti lhs, ResourceState rhs)
{
    lhs |= rhs;
    return lhs;
}

constexpr inline ResourceStateMulti operator&(ResourceStateMulti lhs, ResourceState rhs)
{
    lhs.value &= static_cast<ResourceStateMulti::U>(rhs);
    return lhs;
}

constexpr inline ResourceStateMulti operator&(ResourceStateMulti lhs, ResourceStateMulti rhs)
{
    lhs.value &= rhs.value;
    return lhs;
}

constexpr inline ResourceStateMulti operator|(ResourceState lhs, ResourceState rhs)
{
    ResourceStateMulti lhsM = lhs;
    return lhsM | rhs;
}

using ResourceIndex = uint32_t;