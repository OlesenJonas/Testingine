#pragma once

#include "Texture.hpp"
#include <Datastructures/Pool/Pool.hpp>

struct TextureView
{
    struct CreateInfo
    {
        std::string debugName = ""; // NOLINT
        Texture::Handle parent;
        Texture::Type type = Texture::Type::t2D;
        ResourceStateMulti allStates = ResourceState::None;
        uint32_t baseMipLevel = 0;
        uint32_t mipCount = 1;
        uint32_t baseArrayLayer = 0;
        uint32_t arrayLength = 1;
    };

    struct Descriptor
    {
        Texture::Handle parent;
        Texture::Type type = Texture::Type::t2D;
        ResourceStateMulti allStates = ResourceState::None;
        uint32_t baseMipLevel = 0;
        uint32_t mipCount = 1;
        uint32_t baseArrayLayer = 0;
        uint32_t arrayLength = 1;
    };

    Descriptor descriptor;

    VkImageView imageView = VK_NULL_HANDLE;
    uint32_t resourceIndex = 0xFFFFFFFF;
};