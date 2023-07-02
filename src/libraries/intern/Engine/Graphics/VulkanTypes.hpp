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

struct AllocatedImage
{
    VkImage image;
    VmaAllocation allocation;
};

struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocInfo;
};