#pragma once

#include "Texture/Texture.hpp"

struct RenderTarget
{
    enum struct LoadOp
    {
        Load,
        Clear,
        DontCare,
    };
    enum struct StoreOp
    {
        Store,
        DontCare,
    };
};

struct ColorTarget : public RenderTarget
{
    // dont like this, but it works for now
    struct SwapchainImage
    {
    };
    std::variant<Texture::Handle, SwapchainImage> texture;
    LoadOp loadOp = LoadOp::Load;
    StoreOp storeOp = StoreOp::Store;
};

struct DepthTarget : public RenderTarget
{
    Texture::Handle texture;
    LoadOp loadOp = LoadOp::Load;
    StoreOp storeOp = StoreOp::Store;
};