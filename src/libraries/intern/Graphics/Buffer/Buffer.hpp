#pragma once

#include <VMA/VMA.hpp>

struct Buffer
{
    struct Info
    {
        size_t size = 0;
        VkBufferUsageFlags usage = 0;
        struct MemoryAllocationInfo
        {
            VmaAllocationCreateFlags flags = 0;
            VkMemoryPropertyFlags requiredMemoryPropertyFlags = 0;
        } memoryAllocationInfo;
    };
    struct CreateInfo
    {
        Info info;
        void* initialData = nullptr;
    };

    // todo: should be private and no setter available

    Info info;

    VkBuffer buffer;
    uint32_t uniformResourceIndex = 0xFFFFFFFF;
    uint32_t storageResourceIndex = 0xFFFFFFFF;

    VmaAllocation allocation;
    VmaAllocationInfo allocInfo;
};