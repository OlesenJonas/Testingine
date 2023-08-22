#pragma once

#include <Engine/Graphics/Graphics.hpp>
#include <concepts>
#include <type_traits>
#include <vulkan/vulkan_core.h>

template <typename T>
consteval VkObjectType toVkObjectType()
{
#define VK_OBJ_CASE(Type, E)                                                                                      \
    if constexpr(std::is_same_v<T, Type>)                                                                         \
        return E;

    VK_OBJ_CASE(VkBuffer, VK_OBJECT_TYPE_BUFFER)
    VK_OBJ_CASE(VkImage, VK_OBJECT_TYPE_IMAGE)
    VK_OBJ_CASE(VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW)
    VK_OBJ_CASE(VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE)
    VK_OBJ_CASE(VkDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET)
    VK_OBJ_CASE(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
    VK_OBJ_CASE(VkPipeline, VK_OBJECT_TYPE_PIPELINE)

#undef VK_OBJ_CASE

    return VK_OBJECT_TYPE_UNKNOWN;
}

template <typename T>
concept VulkanConvertible = toVkObjectType<T>() != VK_OBJECT_TYPE_UNKNOWN;

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
    case ResourceState::OldSwapchainImage:
    case ResourceState::PresentSrc:
        assert(false);
        break;
    }
    return 0;
}

VkImageUsageFlags toVkImageUsage(ResourceStateMulti states);

constexpr VkBufferUsageFlags toVkBufferUsageSingle(ResourceState state)
{
    switch(state)
    {
    case ResourceState::None:
    case ResourceState::Undefined:
    case ResourceState::SampleSource:
    case ResourceState::SampleSourceGraphics:
    case ResourceState::SampleSourceCompute:
    case ResourceState::Rendertarget:
    case ResourceState::DepthStencilTarget:
    case ResourceState::DepthStencilReadOnly:
    case ResourceState::OldSwapchainImage:
    case ResourceState::PresentSrc:
        assert(false);
    case ResourceState::TransferSrc:
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    case ResourceState::TransferDst:
        return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case ResourceState::VertexBuffer:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    case ResourceState::IndexBuffer:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case ResourceState::UniformBuffer:
    case ResourceState::UniformBufferGraphics:
    case ResourceState::UniformBufferCompute:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case ResourceState::Storage:
    case ResourceState::StorageGraphics:
    case ResourceState::StorageCompute:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case ResourceState::IndirectArgument:
        return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    return 0;
}

VkBufferUsageFlags toVkBufferUsage(ResourceStateMulti states);

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
        return VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    case ResourceState::IndexBuffer:
        return VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    case ResourceState::IndirectArgument:
        return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    case ResourceState::OldSwapchainImage:
        return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    case ResourceState::PresentSrc:
        return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
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
        // TODO: can remove ATTACHMENT_READ_BIT here?
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
    case ResourceState::OldSwapchainImage:
        return 0;
    case ResourceState::PresentSrc:
        return 0;
    }
}

constexpr VkAttachmentLoadOp toVkLoadOp(const RenderTarget::LoadOp op)
{
    switch(op)
    {
    case RenderTarget::LoadOp::Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case RenderTarget::LoadOp::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case RenderTarget::LoadOp::DontCare:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

constexpr VkAttachmentStoreOp toVkStoreOp(const RenderTarget::StoreOp op)
{
    switch(op)
    {
    case RenderTarget::StoreOp::Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case RenderTarget::StoreOp::DontCare:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
}

#include <Engine/Graphics/Texture/Texture.hpp>

constexpr uint32_t toVkArrayLayers(const Texture::Descriptor& desc)
{
    return desc.type == Texture::Type::tCube ? 6 * desc.arrayLength : desc.arrayLength;
}

constexpr uint32_t toVkArrayLayers(const Texture::CreateInfo& info)
{
    return info.type == Texture::Type::tCube ? 6 * info.arrayLength : info.arrayLength;
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
    case ResourceState::StorageGraphics:
    case ResourceState::StorageCompute:
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

#include <Engine/Graphics/Texture/Sampler.hpp>

constexpr VkFilter toVkFilter(Sampler::Filter filter)
{
    switch(filter)
    {
    case Sampler::Filter::Nearest:
        return VK_FILTER_NEAREST;
    case Sampler::Filter::Linear:
        return VK_FILTER_LINEAR;
    }
}

constexpr VkSamplerMipmapMode toVkMipmapMode(Sampler::Filter filter)
{
    switch(filter)
    {
    case Sampler::Filter::Nearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case Sampler::Filter::Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

constexpr VkSamplerAddressMode toVkAddressMode(Sampler::AddressMode mode)
{
    switch(mode)
    {
    case Sampler::AddressMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case Sampler::AddressMode::ClampEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}

#include <Engine/Graphics/Buffer/Buffer.hpp>

constexpr VmaAllocationCreateInfo toVMAAllocCrInfo(Buffer::MemoryType type)
{
    switch(type)
    {
    case Buffer::MemoryType::CPU:
        return VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
    case Buffer::MemoryType::GPU:
        return VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };
    case Buffer::MemoryType::GPU_BUT_CPU_VISIBLE:
        return VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
    }
}