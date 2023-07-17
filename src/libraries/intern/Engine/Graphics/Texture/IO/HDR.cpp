#include "HDR.hpp"

#include "../Texture.hpp"
#include <Datastructures/Pool.hpp>
#include <stb/stb_image.h>

#include <Engine/Engine.hpp>
#include <iostream>

Handle<Texture> HDR::load(std::string_view path, std::string_view debugName, Texture::Usage usage)
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
    Texture::Format imageFormat = Texture::Format::r32g32b32a32_float;
    Texture::Extent imageExtent{
        .width = (uint32_t)texWidth,
        .height = (uint32_t)texHeight,
        .depth = 1,
    };

    Handle<Texture> newTextureHandle = Engine::get()->getResourceManager()->createTexture(Texture::CreateInfo{
        // specifying non-default values only
        .debugName = std::string{debugName},
        .format = imageFormat,
        .usage = usage | Texture::Usage::TransferDst,
        .size = imageExtent,
        .initialData = {(uint8_t*)pixels, pixelCount * sizeof(float)},
    });

    stbi_image_free(pixels);

    return newTextureHandle;
}