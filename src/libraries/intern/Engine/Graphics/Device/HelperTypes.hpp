#pragma once

// Vulkan related (helper-) types

#include <vulkan/vulkan.h>

#include <VMA/VMA.hpp>

#include <optional>
#include <vector>

struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// TODO:
struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> graphicsAndComputeFamily;
    std::optional<uint32_t> presentFamily;

    [[nodiscard]] inline bool isComplete() const
    {
        return graphicsFamily.has_value() && graphicsAndComputeFamily.has_value() && presentFamily.has_value();
    }
};

#include <memory>

struct VulkanTexture
{
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView fullImageView = VK_NULL_HANDLE;
    uint32_t resourceIndex = 0xFFFFFFFF;

    std::unique_ptr<VkImageView[]> _mipImageViews = VK_NULL_HANDLE;
    std::unique_ptr<uint32_t[]> _mipResourceIndices;
};

struct VulkanBuffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo{};
    uint32_t resourceIndex = 0xFFFFFFFF;
    void* ptr = nullptr;
};

struct VulkanSampler
{
    VkSampler sampler = VK_NULL_HANDLE;
    uint32_t resourceIndex = 0xFFFFFFFF;
};