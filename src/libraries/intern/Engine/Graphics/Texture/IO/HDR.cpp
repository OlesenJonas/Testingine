#include "../Texture.hpp"
#include <stb/stb_image.h>

#include <iostream>

Texture::LoadResult Texture::loadHDR(const Texture::LoadInfo& loadInfo)
{
    int texWidth = 0;
    int texHeight = 0;
    int texChannels = 0;
    float* pixels = stbi_loadf(loadInfo.path.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if(pixels == nullptr)
    {
        std::cout << "Failed to load texture file: " << loadInfo.path << std::endl;
        return {};
    }
    void* pixelPtr = pixels;
    VkDeviceSize pixelCount = texWidth * texHeight * 4; // 4 since load forces rgb_alpha
    Texture::Format imageFormat = Texture::Format::R32_G32_B32_A32_FLOAT;
    Texture::Extent imageExtent{
        .width = (uint32_t)texWidth,
        .height = (uint32_t)texHeight,
        .depth = 1,
    };

    uint32_t maxDimension = std::max(texWidth, texHeight);
    int32_t possibleMipLevels = int32_t(floor(log2(maxDimension))) + 1;
    int32_t mipLevels = -1;
    if(loadInfo.mipLevels == Texture::MipLevels::All)
    {
        mipLevels = possibleMipLevels;
    }
    else if(loadInfo.mipLevels > possibleMipLevels)
    {
        // TODO: LOG: Warn: more requested than possible
        mipLevels = possibleMipLevels;
    }

    return {
        Texture::CreateInfo{
            // specifying non-default values only
            .format = imageFormat,
            .allStates = loadInfo.allStates,
            .initialState = loadInfo.initialState,
            .size = imageExtent,
            .mipLevels = mipLevels,
            .initialData = {(uint8_t*)pixels, pixelCount * sizeof(float)},
        },
        [pixels]()
        {
            stbi_image_free(pixels);
        }};
}