#pragma once

#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Device/HelperTypes.hpp>
#include <Engine/Graphics/Graphics.hpp>
#include <string>

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

    VulkanBuffer gpuBuffer;
};