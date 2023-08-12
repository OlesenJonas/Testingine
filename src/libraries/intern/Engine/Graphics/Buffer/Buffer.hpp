#pragma once

#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Graphics.hpp>
#include <VMA/VMA.hpp>
#include <string>
#include <vulkan/vulkan_core.h>

struct Buffer
{
    enum struct MemoryType
    {
        CPU,
        GPU,
        GPU_BUT_CPU_VISIBLE,
        // TODO: memory type for GPU readback:
        // https://themaister.net/blog/2019/04/17/a-tour-of-granites-vulkan-backend-part-2/#:~:text=Vulkan%3A%3ABufferDomain%3A%3A-,CachedHost,-%E2%80%93%20Must%20be%20HOST_VISIBLE
    };

    struct CreateInfo
    {
        std::string debugName = ""; // NOLINT

        size_t size = 0;
        MemoryType memoryType = MemoryType::GPU;

        ResourceStateMulti allStates = ResourceState::None;
        ResourceState initialState = ResourceState::None;

        Span<uint8_t> initialData;
    };

    struct Descriptor
    {
        size_t size = 0;
        MemoryType memoryType = MemoryType::GPU;

        ResourceStateMulti allStates = ResourceState::None;
    };

    Descriptor descriptor;

    VkBuffer buffer;
    uint32_t resourceIndex = 0xFFFFFFFF;

    VmaAllocation allocation;
    VmaAllocationInfo allocInfo;
    void* ptr = nullptr;
};

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