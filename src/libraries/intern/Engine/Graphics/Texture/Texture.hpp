#pragma once

#include <Datastructures/Pool/Handle.hpp>
#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Device/HelperTypes.hpp>
#include <Engine/Graphics/Graphics.hpp>
#include <Engine/Misc/EnumHelpers.hpp>
#include <functional>
#include <memory>
#include <string>

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
        UNDEFINED,
        R8_UNORM,

        R8_G8_B8_A8_UNORM,
        R8_G8_B8_A8_SRGB,
        B8_G8_R8_A8_SRGB,

        R16_G16_FLOAT,

        R16_G16_B16_A16_FLOAT,
        R32_G32_B32_A32_FLOAT,

        //-- Special Formats
        D32_FLOAT,
    };

    struct LoadInfo
    {
        std::string path;
        std::string_view debugName;
        bool fileDataIsLinear = false;
        int32_t mipLevels = 1;
        bool fillMipLevels = true;
        ResourceStateMulti allStates = ResourceState::None;
        ResourceState initialState = ResourceState::Undefined;

        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
    };

    struct CreateInfo
    {
        std::string debugName = ""; // NOLINT

        Type type = Type::t2D;
        Format format = Format::UNDEFINED;
        ResourceStateMulti allStates = ResourceState::None;
        ResourceState initialState = ResourceState::None;

        Extent size = {1, 1, 1};
        int32_t mipLevels = 1;
        uint32_t arrayLength = 1;

        Span<uint8_t> initialData;
        bool fillMipLevels = true;

        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
    };

    /*
        2nd member is a destroyer function to handle the memory pointed to by inital data inside the create info
    */
    using LoadResult = std::pair<Texture::CreateInfo, std::function<void()>>;

    struct Descriptor
    {
        Type type = Type::t2D;
        Format format = Format::UNDEFINED;
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

    static LoadResult loadHDR(const Texture::LoadInfo& loadInfo);
    static LoadResult loadDefault(const Texture::LoadInfo& loadInfo);

    // -----------------------------------------------------

    // TODO: think about how to split this into pool components, could also duplicate data if it makes sense
    /*
        Descriptor
        VmaAllocation
        VkImage
        VkImageView
        ResourceIndex
    */

    // wrapping in structs like this could allow overloading for other gfx apis
    //      pool internally stores eg Texture::GPUVulkan but pool.get just returns Texture::GPU*
    struct GPU
    {
        VkImage image;
        VkImageView imageView;
    };
    // Own struct since only used on create/destroy
    struct Allocation
    {
        VmaAllocation allocation = VK_NULL_HANDLE;
    };
    // Descriptor
    // ResourceIndex

    using Handle = Handle<std::string, Descriptor, GPU, ResourceIndex, Allocation>;
};
