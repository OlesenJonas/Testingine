#pragma once

#include <vulkan/vulkan_core.h>

struct Material
{
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet textureSet{VK_NULL_HANDLE};
};