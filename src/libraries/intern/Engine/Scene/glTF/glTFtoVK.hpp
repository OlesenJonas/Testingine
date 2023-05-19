#pragma once

#include <vulkan/vulkan_core.h>

namespace glTF::toVk
{
    VkFilter magFilter(uint32_t magFilter);
    VkFilter minFilter(uint32_t minFilter);
    VkSamplerMipmapMode mipmapMode(uint32_t minFilter);
    VkSamplerAddressMode addressMode(uint32_t wrap);
} // namespace glTF::toVk