#pragma once

#include <VMA/VMA.hpp>
#include <vulkan/vulkan_core.h>

#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Graphics.hpp>
#include <Engine/Misc/EnumHelpers.hpp>
#include <functional>
#include <string>

template <typename T>
struct Handle;

struct Texture
{
    struct Extent
    {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
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
    };

    /*
        todo:
            have a global string buffer and store debug name there, then store string_view here
            That way debug name can be retrieved later without storing full string in here
    */

    Descriptor descriptor;

    VkImage image = VK_NULL_HANDLE;
    uint32_t resourceIndex = 0xFFFFFFFF;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;

    static LoadResult loadHDR(Texture::LoadInfo&& loadInfo);
    static LoadResult loadDefault(Texture::LoadInfo&& loadInfo);

    // TODO: not sure I like this being here, maybe some renderer.utils ?
    static void fillMipLevels(Handle<Texture> texture, ResourceState state);
};

// TODO: seperate file!
struct Sampler
{
    struct Info
    {
        VkSamplerCreateFlags flags = 0;
        VkFilter magFilter = VK_FILTER_LINEAR;
        VkFilter minFilter = VK_FILTER_LINEAR;
        VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        float mipLodBias = 0.0f;
        VkBool32 anisotropyEnable = VK_FALSE;
        float maxAnisotropy = 0.0f;
        VkBool32 compareEnable = VK_FALSE;
        VkCompareOp compareOp = VK_COMPARE_OP_NEVER;
        float minLod = 0.0;
        float maxLod = VK_LOD_CLAMP_NONE;
        VkBorderColor borderColor;
        VkBool32 unnormalizedCoordinates = VK_FALSE;
    };
    Info info;
    VkSampler sampler;
    uint32_t resourceIndex = 0xFFFFFFFF;
};
bool operator==(const Sampler::Info& lhs, const Sampler::Info& rhs);