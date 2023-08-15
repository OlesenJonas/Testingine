#pragma once

#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Device/HelperTypes.hpp>
#include <Engine/Graphics/Graphics.hpp>
#include <Engine/Misc/EnumHelpers.hpp>
#include <functional>
#include <memory>
#include <string>

template <typename T>
struct Handle;

struct Texture
{
    struct Extent
    {
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
    };

    enum MipLevels : int32_t
    {
        All = -1,
    };

    enum struct Type
    {
        // identifier cant start with number 😑
        t2D,
        tCube,
        // ...
    };

    // enum struct Format
    enum struct Format
    {
        Undefined,
        r8_unorm,

        r8g8b8a8_unorm,
        r8g8b8a8_srgb,

        r16_g16_float,

        r16g16b16a16_float,
        r32g32b32a32_float,

        //-- Special Formats
        d32_float,
    };

    struct LoadInfo
    {
        std::string_view path;
        std::string_view debugName;
        bool fileDataIsLinear = false;
        int32_t mipLevels = 1;
        bool fillMipLevels = true;
        ResourceStateMulti allStates = ResourceState::None;
        ResourceState initialState = ResourceState::Undefined;
    };

    struct CreateInfo
    {
        std::string debugName = ""; // NOLINT

        Type type = Type::t2D;
        Format format = Format::Undefined;
        ResourceStateMulti allStates = ResourceState::None;
        ResourceState initialState = ResourceState::None;

        Extent size = {1, 1, 1};
        int32_t mipLevels = 1;
        uint32_t arrayLength = 1;

        Span<uint8_t> initialData;
        bool fillMipLevels = true;
    };

    /*
        2nd member is a destroyer function to handle the memory pointed to by inital data inside the create info
    */
    using LoadResult = std::pair<Texture::CreateInfo, std::function<void()>>;

    struct Descriptor
    {
        Type type = Type::t2D;
        Format format = Format::Undefined;
        ResourceStateMulti allStates = ResourceState::None;

        Extent size = {1, 1, 1};
        int32_t mipLevels = 1;
        uint32_t arrayLength = 1;

        constexpr static Descriptor fromCreateInfo(const CreateInfo& crInfo)
        {
            return Descriptor{
                .type = crInfo.type,
                .format = crInfo.format,
                .allStates = crInfo.allStates,
                .size = crInfo.size,
                .mipLevels = crInfo.mipLevels,
                .arrayLength = crInfo.arrayLength,
            };
        }
    };

    static LoadResult loadHDR(Texture::LoadInfo&& loadInfo);
    static LoadResult loadDefault(Texture::LoadInfo&& loadInfo);

    uint32_t fullResourceIndex() const;
    uint32_t mipResourceIndex(uint32_t level) const;
    // TODO: own textureView abstraction
    VkImageView fullResourceView() const;
    VkImageView mipResourceView(uint32_t level) const;

    Descriptor descriptor;
    VulkanTexture gpuTexture;
};