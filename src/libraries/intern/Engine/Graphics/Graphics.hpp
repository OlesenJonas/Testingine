#pragma once
#include <Engine/Misc/EnumHelpers.hpp>
#include <cstdint>
#include <type_traits>

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
    StorageRead          = nthBit(5u),
    StorageWrite         = nthBit(6u),
    StorageGraphics      = nthBit(7u),
    StorageGraphicsRead  = nthBit(8u),
    StorageGraphicsWrite = nthBit(9u),
    StorageCompute       = nthBit(10u),
    StorageComputeRead   = nthBit(11u),
    StorageComputeWrite  = nthBit(12u),

    // Texture specific
    SampleSource         = nthBit(13u),
    SampleSourceGraphics = nthBit(14u),
    SampleSourceCompute  = nthBit(15u),

    Rendertarget         = nthBit(16u),
    DepthStencilTarget   = nthBit(17u),
    DepthStencilReadOnly = nthBit(18u),

    // Buffer specific
    VertexBuffer          = nthBit(19u),
    IndexBuffer           = nthBit(20u),
    UniformBuffer         = nthBit(21u),
    UniformBufferGraphics = nthBit(22u),
    UniformBufferCompute  = nthBit(23u),
    IndirectArgument      = nthBit(24u),

    // Swapchain specific
    OldSwapchainImage     = nthBit(25u),
    PresentSrc            = nthBit(26u),
};
// clang-format on

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

#include <Datastructures/Pool.hpp>
#include <variant>
struct Texture;
struct RenderTarget
{
    // dont like this, but it works for now
    struct SwapchainImage
    {
    };
    std::variant<Handle<Texture>, SwapchainImage> texture;

    enum struct LoadOp
    {
        Load,
        Clear,
        DontCare,
    };
    LoadOp loadOp = LoadOp::Load;

    enum struct StoreOp
    {
        Store,
        DontCare,
    };
    StoreOp storeOp = StoreOp::Store;
};