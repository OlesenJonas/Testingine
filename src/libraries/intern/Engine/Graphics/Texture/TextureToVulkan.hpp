#pragma once

#include "Texture.hpp"
#include <Engine/Graphics/Graphics.hpp>
#include <bit>
#include <vulkan/vulkan_core.h>

/*
    Should this be here, or next to the vulkan renderer?
    TODO:   I think this should be next to the other conversion functions!
*/

constexpr uint32_t toVkArrayLayers(const Texture::Descriptor& desc)
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
        assert(false && "Unhandled type to convert");
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
    case Texture::Format::r16_g16_float:
        return VK_FORMAT_R16G16_SFLOAT;
    case Texture::Format::r16g16b16a16_float:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Texture::Format::r32g32b32a32_float:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Texture::Format::d32_float:
        return VK_FORMAT_D32_SFLOAT;
    default:
        assert(false && "Unhandled formats to convert");
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
    case Texture::Format::r16_g16_float:
    case Texture::Format::r16g16b16a16_float:
    case Texture::Format::r32g32b32a32_float:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case Texture::Format::d32_float:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
        assert(false && "Unhandled formats to convert");
    }
}

constexpr VkImageLayout toVkImageLayout(ResourceState state)
{
    switch(state)
    {
    case ResourceState::None:
        assert(false);
    case ResourceState::Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ResourceState::TransferSrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ResourceState::TransferDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ResourceState::Storage:
    case ResourceState::StorageRead:
    case ResourceState::StorageWrite:
    case ResourceState::StorageGraphics:
    case ResourceState::StorageGraphicsRead:
    case ResourceState::StorageGraphicsWrite:
    case ResourceState::StorageCompute:
    case ResourceState::StorageComputeRead:
    case ResourceState::StorageComputeWrite:
        return VK_IMAGE_LAYOUT_GENERAL;
    case ResourceState::SampleSource:
    case ResourceState::SampleSourceGraphics:
    case ResourceState::SampleSourceCompute:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ResourceState::Rendertarget:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ResourceState::DepthStencilTarget:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ResourceState::DepthStencilReadOnly:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case ResourceState::VertexBuffer:
    case ResourceState::IndexBuffer:
    case ResourceState::UniformBuffer:
    case ResourceState::UniformBufferGraphics:
    case ResourceState::UniformBufferCompute:
    case ResourceState::IndirectArgument:
        assert(false && "Invalid resource state for texture!");
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ResourceState::OldSwapchainImage:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ResourceState::PresentSrc:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
}
