#pragma once

#include <intern/Datastructures/Pool.hpp>
#include <vulkan/vulkan_core.h>

struct GraphicsPipeline;

struct Material
{
    Handle<GraphicsPipeline> pipeline;
    VkDescriptorSet textureSet{VK_NULL_HANDLE};
};