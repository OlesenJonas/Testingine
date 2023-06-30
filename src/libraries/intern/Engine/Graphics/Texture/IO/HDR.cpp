#include "HDR.hpp"

#include "../Texture.hpp"
#include <Datastructures/Pool.hpp>
#include <stb/stb_image.h>

#include <Engine/Engine.hpp>
#include <iostream>

Handle<Texture> HDR::load(std::string_view path, std::string_view debugName, VkImageUsageFlags usage)
{
    int texWidth = 0;
    int texHeight = 0;
    int texChannels = 0;
    float* pixels = stbi_loadf(path.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if(pixels == nullptr)
    {
        std::cout << "Failed to load texture file: " << path << std::endl;
        return {};
    }
    void* pixelPtr = pixels;
    VkDeviceSize pixelCount = texWidth * texHeight * 4; // 4 since load forces rgb_alpha
    VkFormat imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkExtent3D imageExtent{
        .width = (uint32_t)texWidth,
        .height = (uint32_t)texHeight,
        .depth = 1,
    };

    Handle<Texture> newTextureHandle = Engine::get()->getResourceManager()->createTexture(Texture::Info{
        // specifying non-default values only
        .debugName = std::string{debugName},
        .size = imageExtent,
        .format = imageFormat,
        .usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialData = {(uint8_t*)pixels, pixelCount * sizeof(float)},
    });

    stbi_image_free(pixels);

    return newTextureHandle;
}