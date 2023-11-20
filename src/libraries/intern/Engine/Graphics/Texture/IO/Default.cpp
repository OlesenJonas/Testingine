#include "../Texture.hpp"
#include <stb/stb_image.h>

#include <iostream>

Texture::LoadResult Texture::loadDefault(const Texture::LoadInfo& loadInfo)
{
    int texWidth = 0;
    int texHeight = 0;
    int texChannels = 0;
    stbi_uc* pixels = stbi_load(loadInfo.path.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if(pixels == nullptr)
    {
        std::cout << "Failed to load texture file: " << loadInfo.path << std::endl;
        return {};
    }
    void* pixelPtr = pixels;
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    Texture::Format imageFormat =
        loadInfo.fileDataIsLinear ? Texture::Format::R8_G8_B8_A8_UNORM : Texture::Format::R8_G8_B8_A8_SRGB;
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
            .initialData = {pixels, imageSize},
        },
        [pixels]()
        {
            stbi_image_free(pixels);
        }};
}