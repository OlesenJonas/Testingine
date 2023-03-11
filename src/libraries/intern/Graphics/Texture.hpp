#pragma once

#include "VulkanTypes.hpp"

struct Texture
{
    AllocatedImage image;
    VkImageView imageView;
};

struct VulkanRenderer;

// TODO: dont like VulkanRenderer parameter here, instead grab from current context!
bool loadImageFromFile(VulkanRenderer& renderer, const char* file, AllocatedImage& image);