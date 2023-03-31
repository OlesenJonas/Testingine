#pragma once

#include <VMA/VMA.hpp>
#include <vulkan/vulkan_core.h>

struct Texture
{
    struct Info
    {
        // Doesnt really have any options atm since most stuff is still hardcoded
        VkExtent3D size = {1, 1, 1};
        VkImageUsageFlags usage = 0;
    };
    struct CreateInfo
    {
        Info info;
        void* initialData = nullptr;
    };

    // todo: should be private and no setter available

    Info info;
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
};