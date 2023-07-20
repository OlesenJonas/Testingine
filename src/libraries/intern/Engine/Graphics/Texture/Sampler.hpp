#pragma once

#include <vulkan/vulkan_core.h>

struct Sampler
{
    enum struct Filter
    {
        Nearest,
        Linear,
    };

    enum struct AddressMode
    {
        Repeat,
        ClampEdge,
    };

    static constexpr float LodMax = 1000.0f;

    struct Info
    {
        Filter magFilter = Filter::Linear;
        Filter minFilter = Filter::Linear;
        Filter mipMapFilter = Filter::Linear;
        AddressMode addressModeU = AddressMode::Repeat;
        AddressMode addressModeV = AddressMode::Repeat;
        AddressMode addressModeW = AddressMode::Repeat;
        bool enableAnisotropy = false;
        float minLod = 0.0;
        float maxLod = LodMax;

        bool operator==(const Info& rhs) const;
    };
    Info info;
    VkSampler sampler = VK_NULL_HANDLE;
    uint32_t resourceIndex = 0xFFFFFFFF;
};