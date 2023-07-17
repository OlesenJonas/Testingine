#pragma once

#include "Texture.hpp"
#include <bit>
#include <vulkan/vulkan_core.h>

/*
    Should this be here, or next to the vulkan renderer?
*/

constexpr uint32_t toArrayLayers(const Texture::Descriptor& desc)
{
    return desc.type == Texture::Type::tCube ? 6 * desc.arrayLength : desc.arrayLength;
}

constexpr VkImageType toVkImgType(Texture::Type type)
{
    switch(type)
    {
    case Texture::Type::t2D:
        return VK_IMAGE_TYPE_2D;
    case Texture::Type::tCube:
        return VK_IMAGE_TYPE_2D;
    default:
        assert(false && "Unhandeled type to convert");
    }
}

constexpr VkFormat toVkFormat(Texture::Format format)
{
    switch(format)
    {
    case Texture::Format::Undefined:
        return VK_FORMAT_UNDEFINED;
    case Texture::Format::r8_unorm:
        return VK_FORMAT_R8_UNORM;
    case Texture::Format::r8g8b8a8_unorm:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case Texture::Format::r8g8b8a8_srgb:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case Texture::Format::r16g16b16a16_float:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Texture::Format::r32g32b32a32_float:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Texture::Format::d32_float:
        return VK_FORMAT_D32_SFLOAT;
    default:
        assert(false && "Unhandeled formats to convert");
    }
}

constexpr VkImageAspectFlags toVkImageAspect(Texture::Format format)
{
    switch(format)
    {
    case Texture::Format::Undefined:
    case Texture::Format::r8_unorm:
    case Texture::Format::r8g8b8a8_unorm:
    case Texture::Format::r8g8b8a8_srgb:
    case Texture::Format::r16g16b16a16_float:
    case Texture::Format::r32g32b32a32_float:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case Texture::Format::d32_float:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
        assert(false && "Unhandeled formats to convert");
    }
}

constexpr VkImageUsageFlags toVkUsageSingle(Texture::Usage usage)
{
    switch(usage)
    {
    case Texture::Usage::Sampled:
        return VK_IMAGE_USAGE_SAMPLED_BIT;
    case Texture::Usage::Storage:
        return VK_IMAGE_USAGE_STORAGE_BIT;
    case Texture::Usage::ColorAttachment:
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    case Texture::Usage::DepthStencilAttachment:
        return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    case Texture::Usage::TransferSrc:
        return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    case Texture::Usage::TransferDst:
        return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    default:
        assert(false && "Unhandeled usages to convert");
    }
}

VkImageUsageFlags toVkUsage(Texture::Usage usage);
