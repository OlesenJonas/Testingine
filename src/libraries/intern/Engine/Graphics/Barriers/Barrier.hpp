#pragma once

#include <Datastructures/Span.hpp>

#include <vulkan/vulkan_core.h>

// todo: namespace or something!

// very lightweight abstraction, really hides just the dependencyInfo "reroute"
void insertImageMemoryBarriers(VkCommandBuffer cmd, const Span<const VkImageMemoryBarrier2> imageMemoryBarriers);