#pragma once

#include <VMA/VMA.hpp>
#include <vulkan/vulkan_core.h>

struct Texture
{
    struct Info
    {
        VkExtent3D size = {1, 1, 1};
        VkFormat format;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageUsageFlags usage = 0;

        // todo: maybe create Texture3D, TextureArray etc C++ classes
        //       and only store the relevant fields in those
        VkImageType imageType = VK_IMAGE_TYPE_2D;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;

        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

        // TODO: split completly, have image view as seperate object!
        VkImageAspectFlags viewAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    };

    // todo: should be private and no setter available

    Info info;
    VkImage image = VK_NULL_HANDLE;
    uint32_t sampledResourceIndex = 0xFFFFFFFF;
    uint32_t storageResourceIndex = 0xFFFFFFFF;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
};