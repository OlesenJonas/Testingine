#pragma once

#include "Texture.hpp"
#include <Datastructures/Pool/Pool.hpp>

struct TextureView
{
    struct Info
    {
        VkImageViewType textureType = VK_IMAGE_VIEW_TYPE_2D;
        VkImageSubresourceRange subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        // if this is undefined, it will be set to match the given texture
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkComponentMapping componentMapping = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        };
    };

    Handle<Texture> texture;
    Info info;
    VkImageView imageView = VK_NULL_HANDLE;
};