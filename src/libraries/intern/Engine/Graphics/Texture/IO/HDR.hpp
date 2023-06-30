#pragma once

#include <string_view>
#include <vulkan/vulkan_core.h>

struct Texture;
template <typename T>
class Handle;

namespace HDR
{
    Handle<Texture> load(std::string_view path, std::string_view debugName, VkImageUsageFlags usage);
}