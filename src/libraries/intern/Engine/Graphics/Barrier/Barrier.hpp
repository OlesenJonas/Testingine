#pragma once

#include "../Buffer/Buffer.hpp"
#include "../Texture/Texture.hpp"
#include <Datastructures/Pool/Pool.hpp>
#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Graphics.hpp>

/*
    Based on WickedEngine's barrier abstraction
        https://github.com/turanszkij/WickedEngine/blob/ebaef61435e2277af72f8150aaec8eedb43c16be/WickedEngine/wiGraphics.h#L650
        https://github.com/turanszkij/WickedEngine/blob/ebaef61435e2277af72f8150aaec8eedb43c16be/WickedEngine/wiGraphicsDevice_Vulkan.cpp#L8515
*/

/*
    Basic pipeline barries, non-existing parameters are deduced based on the given ones
    (eg old image layout implies certain pipeline stages)
    This is most likely suboptimal in quite a few cases, but its way easier to use

    TODO:
        - offer a more detailed API for creating barriers
            could just add memebers for stages and queues and let user manually create a Barrier object instead
            of using Image/Buffer... functions
        - not sure if passing Handles<> is nice here, its definitly easier to use in the highlevel code
            but it also requires more work when constructing the barriers (potentially multiple times)
            Will have to see and evaluate
*/
struct Barrier
{
    enum class Type
    {
        // Global, //TODO: Implement when I come across a use case for this
        Image,
        Buffer,
    };
    Type type = Type::Buffer;

    struct FromImage
    {
        Texture::Handle texture;
        ResourceState stateBefore = ResourceState::None;
        ResourceState stateAfter = ResourceState::None;
        int32_t mipLevel = 0;
        int32_t mipCount = Texture::MipLevels::All;
        int32_t arrayLayer = 0;
        int32_t arrayLength = 1;
        bool allowDiscardOriginal = false;
    };

    struct FromBuffer
    {
        Handle<Buffer> buffer;
        ResourceState stateBefore = ResourceState::None;
        ResourceState stateAfter = ResourceState::None;
        size_t offset = 0;
        size_t size = VK_WHOLE_SIZE;
    };

    union
    {
        FromImage image;
        FromBuffer buffer;
    };

    Barrier(FromImage&& barrierInfo) { type = Type::Image, image = barrierInfo; }
    Barrier(FromBuffer&& barrierInfo) { type = Type::Buffer, buffer = barrierInfo; }
};