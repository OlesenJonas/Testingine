#include "Texture.hpp"
#include "Graphics/Renderer/VulkanRenderer.hpp"
#include "Graphics/Texture/Texture.hpp"
#include "IO/HDR.hpp"
#include "TexToVulkan.hpp"
#include <Engine/Engine.hpp>
#include <Engine/Graphics/Barriers/Barrier.hpp>
#include <Engine/Graphics/Renderer/VulkanDebug.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <format>
#include <iostream>
#include <stb/stb_image.h>
#include <vulkan/vulkan_core.h>

Handle<Texture>
ResourceManager::createTexture(const char* file, Texture::Usage usage, bool dataIsLinear, std::string_view name)
{
    std::string_view fileView{file};
    auto lastDirSep = fileView.find_last_of("/\\");
    auto extensionStart = fileView.find_last_of('.');
    std::string_view texName =
        name.empty() ? fileView.substr(lastDirSep + 1, extensionStart - lastDirSep - 1) : name;

    // todo: handle naming collisions
    auto iterator = nameToMeshLUT.find(texName);
    assert(iterator == nameToMeshLUT.end());

    std::string_view extension = fileView.substr(extensionStart);

    if(extension == ".hdr")
    {
        return HDR::load(fileView, texName, usage);
    }

    // else just use default stbi_load for now
    //   TODO: refactor and split into multiple functions

    int texWidth = 0;
    int texHeight = 0;
    int texChannels = 0;
    stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if(pixels == nullptr)
    {
        std::cout << "Failed to load texture file: " << file << std::endl;
        return {};
    }
    void* pixelPtr = pixels;
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    Texture::Format imageFormat = dataIsLinear ? Texture::Format::r8g8b8a8_unorm : Texture::Format::r8g8b8a8_srgb;
    Texture::Extent imageExtent{
        .width = (uint32_t)texWidth,
        .height = (uint32_t)texHeight,
        .depth = 1,
    };

    Handle<Texture> newTextureHandle = createTexture(Texture::CreateInfo{
        // specifying non-default values only
        .debugName = std::string{texName},
        .format = imageFormat,
        .usage = usage | Texture::Usage::TransferDst,
        .size = imageExtent,
        .initialData = {pixels, imageSize},
    });

    stbi_image_free(pixels);

    return newTextureHandle;
}

Handle<Texture> ResourceManager::createTexture(Texture::CreateInfo&& createInfo)
{
    // todo: handle naming collisions and also dont just use a random name...
    std::string name = createInfo.debugName;
    if(name.empty())
    {
        name = "Texture" + std::format("{:03}", rand() % 199);
    }
    auto iterator = nameToMeshLUT.find(name);
    assert(iterator == nameToMeshLUT.end());

    const uint32_t maxDimension = std::max({createInfo.size.width, createInfo.size.height, createInfo.size.depth});
    uint32_t actualMipLevels = createInfo.mipLevels == Texture::MipLevels::All
                                   ? uint32_t(floor(log2(maxDimension))) + 1
                                   : createInfo.mipLevels;
    assert(actualMipLevels > 0);
    createInfo.mipLevels = actualMipLevels;

    Handle<Texture> newTextureHandle = texturePool.insert(Texture{
        .descriptor = {
            .type = createInfo.type,
            .format = createInfo.format,
            .usage = createInfo.usage,
            .size = createInfo.size,
            .mipLevels = createInfo.mipLevels,
            .arrayLength = createInfo.arrayLength,
        }});
    Texture* tex = texturePool.get(newTextureHandle);
    assert(tex);

    VkImageCreateFlags flags = createInfo.type == Texture::Type::tCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    uint32_t arrayLayers = toArrayLayers(tex->descriptor);

    VkImageCreateInfo imageCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,

        .imageType = toVkImgType(createInfo.type),
        .format = toVkFormat(createInfo.format),
        .extent =
            {
                .width = createInfo.size.width,
                .height = createInfo.size.height,
                .depth = createInfo.size.depth,
            },

        .mipLevels = static_cast<uint32_t>(createInfo.mipLevels),
        .arrayLayers = arrayLayers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = toVkUsage(createInfo.usage),
    };
    VmaAllocationCreateInfo imgAllocInfo{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    VmaAllocator& allocator = Engine::get()->getRenderer()->allocator;
    vmaCreateImage(allocator, &imageCrInfo, &imgAllocInfo, &tex->image, &tex->allocation, nullptr);

    if(!createInfo.initialData.empty())
    {
        /*
            TODO:
                rewrite all of the data upload functionality (also in create mesh etc)
                Have one large staging buffer in CPU memory that just gets reused all the time
                while handing out chunks and tracking when theyre done being used.
                Also read book section about async transfer queue
        */
        /*
            Upload the pixel data through staging buffer, see the createBuffer(...) function for todos on how to
            improve this
        */

        // allocate staging buffer
        VkBufferCreateInfo stagingBufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = createInfo.initialData.size(),
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        VmaAllocationCreateInfo vmaallocCrInfo = {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        AllocatedBuffer stagingBuffer;
        VkResult res = vmaCreateBuffer(
            Engine::get()->getRenderer()->allocator,
            &stagingBufferInfo,
            &vmaallocCrInfo,
            &stagingBuffer.buffer,
            &stagingBuffer.allocation,
            &stagingBuffer.allocInfo);
        assert(res == VK_SUCCESS);

        void* data = nullptr;
        vmaMapMemory(allocator, stagingBuffer.allocation, &data);
        memcpy(data, createInfo.initialData.data(), createInfo.initialData.size());
        vmaUnmapMemory(allocator, stagingBuffer.allocation);

        Engine::get()->getRenderer()->immediateSubmit(
            [&](VkCommandBuffer cmd)
            {
                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = newTextureHandle,
                            .stateBefore = ResourceState::Undefined,
                            .stateAfter = ResourceState::TransferDst,
                        }),
                    });

                VkBufferImageCopy copyRegion{
                    .bufferOffset = 0,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource =
                        {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                         .mipLevel = 0,
                         .baseArrayLayer = 0,
                         .layerCount = 1},
                    .imageExtent =
                        {
                            .width = createInfo.size.width,
                            .height = createInfo.size.height,
                            .depth = createInfo.size.depth,
                        },
                };

                vkCmdCopyBufferToImage(
                    cmd, stagingBuffer.buffer, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = newTextureHandle,
                            .stateBefore = ResourceState::TransferDst,
                            .stateAfter = ResourceState::SampleSource,
                        }),
                    });
            });

        // immediate submit also waits on device idle so staging buffer is not being used here anymore
        vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }

    nameToTextureLUT.insert({std::string{name}, newTextureHandle});

    // create an image view covering the whole image
    VkImageViewType viewType =
        createInfo.type == Texture::Type::tCube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    VkImageViewCreateInfo imageViewCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = tex->image,
        .viewType = viewType,
        .format = toVkFormat(createInfo.format),
        .components =
            VkComponentMapping{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = toVkImageAspect(createInfo.format),
                .baseMipLevel = 0,
                .levelCount = imageCrInfo.mipLevels,
                .baseArrayLayer = 0,
                .layerCount = arrayLayers,
            },
    };

    vkCreateImageView(Engine::get()->getRenderer()->device, &imageViewCrInfo, nullptr, &tex->imageView);

    setDebugName(tex->image, (std::string{name} + "_image").c_str());
    setDebugName(tex->imageView, (std::string{name} + "_mainView").c_str());

    VulkanRenderer& renderer = *VulkanRenderer::get();
    bool usedForSampling = (createInfo.usage & Texture::Usage::Sampled) != 0u;
    bool usedForStorage = (createInfo.usage & Texture::Usage::Storage) != 0u;

    if(usedForSampling && usedForStorage)
    {
        tex->resourceIndex =
            renderer.bindlessManager.createImageBinding(tex->imageView, BindlessManager::ImageUsage::Both);
    }
    // else: not both but maybe one of the two
    else if(usedForSampling)
    {
        tex->resourceIndex =
            renderer.bindlessManager.createImageBinding(tex->imageView, BindlessManager::ImageUsage::Sampled);
    }
    else if(usedForStorage)
    {
        tex->resourceIndex =
            renderer.bindlessManager.createImageBinding(tex->imageView, BindlessManager::ImageUsage::Storage);
    }

    return newTextureHandle;
}

Handle<Texture> ResourceManager::createCubemapFromEquirectangular(
    uint32_t cubeResolution, Handle<Texture> equirectangularSource, std::string_view debugName)
{
    auto iterator = nameToMeshLUT.find(debugName);
    assert(iterator == nameToMeshLUT.end());

    Texture* sourceTex = get(equirectangularSource);

    Handle<Texture> cubeTextureHandle = createTexture(Texture::CreateInfo{
        // specifying non-default values only
        .debugName = std::string{debugName},
        .type = Texture::Type::tCube,
        .format = sourceTex->descriptor.format,
        // TRANSFER_XXX_BITs are needed for blitting to downscale :/
        .usage = Texture::Usage::Sampled | Texture::Usage::Storage | Texture::Usage::TransferDst |
                 Texture::Usage::TransferSrc,
        .size = {.width = cubeResolution, .height = cubeResolution, .depth = 1},
        .mipLevels = Texture::MipLevels::All,
    });

    auto linearSampler = createSampler({
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    });

    // re-retrieve in case last creation resized storage
    sourceTex = get(equirectangularSource);
    Texture* cubeTex = get(cubeTextureHandle);

    // TODO: !!!! DONT CREATE HERE !!! CREATE ON INIT() OF ENGINE AND JUST GET BY NAME OR SO HERE !
    // Handle<ComputeShader> conversionShaderHandle =
    //     createComputeShader(SHADERS_PATH "/equiToCube.comp", "equiToCubeCompute");
    Handle<ComputeShader> conversionShaderHandle = createComputeShader(
        {
            .sourcePath = SHADERS_PATH "/Skybox/equiToCube.hlsl",
            .sourceLanguage = Shaders::Language::HLSL,
        },
        "equiToCubeCompute");
    ComputeShader* conversionShader = get(conversionShaderHandle);

    VulkanRenderer* renderer = Engine::get()->getRenderer();
    struct ConversionPushConstants
    {
        uint32_t sourceIndex;
        uint32_t samplerIndex;
        uint32_t targetIndex;
    };
    ConversionPushConstants constants{
        .sourceIndex = sourceTex->resourceIndex,
        .samplerIndex = get(linearSampler)->resourceIndex,
        .targetIndex = cubeTex->resourceIndex,
    };
    renderer->immediateSubmit(
        [=](VkCommandBuffer cmd)
        {
            submitBarriers(
                cmd,
                {
                    Barrier::from(Barrier::Image{
                        .texture = cubeTextureHandle,
                        .stateBefore = ResourceState::Undefined,
                        .stateAfter = ResourceState::StorageCompute,
                    }),
                });

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, conversionShader->pipeline);
            // Bind the bindless descriptor sets once per cmdbuffer
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                renderer->bindlessPipelineLayout,
                0,
                renderer->bindlessManager.getDescriptorSetsCount(),
                renderer->bindlessManager.getDescriptorSets(),
                0,
                nullptr);

            // TODO: CORRECT PUSH CONSTANT RANGES FOR COMPUTE PIPELINES !!!!!
            vkCmdPushConstants(
                cmd,
                renderer->bindlessPipelineLayout,
                VK_SHADER_STAGE_ALL,
                0,
                sizeof(ConversionPushConstants),
                &constants);

            // TODO: dont hardcode sizes! retrieve programmatically
            //       (workrgoup size form spirv, use UintDivAndCeil)
            vkCmdDispatch(cmd, cubeResolution / 8, cubeResolution / 8, 6);

            submitBarriers(
                cmd,
                {
                    Barrier::from(Barrier::Image{
                        .texture = cubeTextureHandle,
                        .stateBefore = ResourceState::StorageCompute,
                        .stateAfter = ResourceState::SampleSource,
                    }),
                });
        });

    Texture::fillMipLevels(cubeTextureHandle);

    return cubeTextureHandle;
}

void ResourceManager::free(Handle<Texture> handle)
{
    /*
        todo:
        Enqueue into a per frame queue
        also need to ensure renderer deletes !all! objects from per frame queue during shutdown, ignoring the
            current frame!
    */
    const Texture* texture = texturePool.get(handle);
    if(texture == nullptr)
    {
        return;
    }
    const VmaAllocator* allocator = &Engine::get()->getRenderer()->allocator;
    VkImage image = texture->image;
    VkImageView imageView = texture->imageView;
    VmaAllocation vmaAllocation = texture->allocation;
    VkDevice device = Engine::get()->getRenderer()->device;
    auto& renderer = *Engine::get()->getRenderer();
    renderer.deleteQueue.pushBack([=]() { vmaDestroyImage(renderer.allocator, image, vmaAllocation); });
    renderer.deleteQueue.pushBack([=]() { vkDestroyImageView(device, imageView, nullptr); });

    texturePool.remove(handle);
}

Handle<Sampler> ResourceManager::createSampler(Sampler::Info&& info)
{
    // Check if such a sampler already exists

    // since the sampler limit is quite low atm, and sampler creation should be a rare thing
    // just doing a linear search here. Could of course hash the creation info and do some lookup
    // in the future

    for(uint32_t i = 0; i < freeSamplerIndex; i++)
    {
        Sampler::Info& entryInfo = samplerArray[i].info;
        if(entryInfo == info)
        {
            return Handle<Sampler>{i, 1};
        }
    }

    // Otherwise create a new sampler

    Handle<Sampler> samplerHandle{freeSamplerIndex, 1};
    freeSamplerIndex++;
    Sampler* sampler = &samplerArray[samplerHandle.index];

    VulkanRenderer& renderer = *VulkanRenderer::get();

    VkSamplerCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = info.flags,
        .magFilter = info.magFilter,
        .minFilter = info.minFilter,
        .mipmapMode = info.mipmapMode,
        .addressModeU = info.addressModeU,
        .addressModeV = info.addressModeV,
        .addressModeW = info.addressModeW,
        .mipLodBias = info.mipLodBias,
        .anisotropyEnable = info.anisotropyEnable,
        .maxAnisotropy = info.maxAnisotropy,
        .compareEnable = info.compareEnable,
        .compareOp = info.compareOp,
        .minLod = info.minLod,
        .maxLod = info.maxLod,
        .borderColor = info.borderColor,
        .unnormalizedCoordinates = info.unnormalizedCoordinates,
    };
    sampler->info = info;

    vkCreateSampler(renderer.device, &createInfo, nullptr, &sampler->sampler);

    // setDebugName(tex->imageView, (std::string{name} + "_mainView").c_str());

    sampler->resourceIndex = renderer.bindlessManager.createSamplerBinding(sampler->sampler, samplerHandle.index);

    return samplerHandle;
}

bool operator==(const Sampler::Info& lhs, const Sampler::Info& rhs)
{
#define EQUAL_MEMBER(member) lhs.member == rhs.member
    return EQUAL_MEMBER(flags) &&            //
           EQUAL_MEMBER(magFilter) &&        //
           EQUAL_MEMBER(minFilter) &&        //
           EQUAL_MEMBER(mipmapMode) &&       //
           EQUAL_MEMBER(addressModeU) &&     //
           EQUAL_MEMBER(addressModeV) &&     //
           EQUAL_MEMBER(addressModeW) &&     //
           EQUAL_MEMBER(mipLodBias) &&       //
           EQUAL_MEMBER(anisotropyEnable) && //
           EQUAL_MEMBER(maxAnisotropy) &&    //
           EQUAL_MEMBER(compareEnable) &&    //
           EQUAL_MEMBER(compareOp) &&        //
           EQUAL_MEMBER(minLod) &&           //
           EQUAL_MEMBER(maxLod) &&           //
           EQUAL_MEMBER(borderColor) &&      //
           EQUAL_MEMBER(unnormalizedCoordinates);
#undef EQUAL_MEMBER
}

void Texture::fillMipLevels(Handle<Texture> texture)
{
    /*
        TODO: check that image was created with usage_transfer_src_bit !
              Switch to compute shader based solution? Could get rid of needing to mark
              *all* textures as transfer_src/dst, just because mips are needed.
              But requires a lot more work to handle different texture types (3d, cube, array)
    */

    auto* rm = Engine::get()->getResourceManager();
    auto* renderer = Engine::get()->getRenderer();

    Texture* tex = rm->get(texture);

    if(tex->descriptor.mipLevels == 1)
        return;

    uint32_t startWidth = tex->descriptor.size.width;
    uint32_t startHeight = tex->descriptor.size.height;
    uint32_t startDepth = tex->descriptor.size.depth;

    renderer->immediateSubmit(
        [=](VkCommandBuffer cmd)
        {
            // Transfer 1st mip to transfer source
            submitBarriers(
                cmd,
                {
                    Barrier::from(Barrier::Image{
                        .texture = texture,
                        // TODO: need better way to determine initial state, pass as parameter?
                        .stateBefore = ResourceState::SampleSource,
                        .stateAfter = ResourceState::TransferSrc,
                    }),
                });

            // Generate mip chain
            // Downscaling from each level successively, but could also downscale mip 0 -> mip N each time

            for(int i = 1; i < tex->descriptor.mipLevels; i++)
            {
                // prepare current level to be transfer dst
                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = texture,
                            // TODO: need better way to determine initial state, pass as parameter?
                            .stateBefore = ResourceState::TransferSrc,
                            .stateAfter = ResourceState::TransferDst,
                            .mipLevel = i,
                            .arrayLength = int32_t(tex->descriptor.arrayLength),
                        }),
                    });

                uint32_t lastWidth = std::max(startWidth >> (i - 1), 1u);
                uint32_t lastHeight = std::max(startHeight >> (i - 1), 1u);
                uint32_t lastDepth = std::max(startDepth >> (i - 1), 1u);

                uint32_t curWidth = std::max(startWidth >> i, 1u);
                uint32_t curHeight = std::max(startHeight >> i, 1u);
                uint32_t curDepth = std::max(startDepth >> i, 1u);

                VkImageBlit blit{
                    .srcSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = uint32_t(i - 1),
                            .baseArrayLayer = 0,
                            .layerCount = toArrayLayers(tex->descriptor),
                        },
                    .srcOffsets =
                        {{.x = 0, .y = 0, .z = 0}, {int32_t(lastWidth), int32_t(lastHeight), int32_t(lastDepth)}},
                    .dstSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = uint32_t(i),
                            .baseArrayLayer = 0,
                            .layerCount = toArrayLayers(tex->descriptor),
                        },
                    .dstOffsets =
                        {{.x = 0, .y = 0, .z = 0}, {int32_t(curWidth), int32_t(curHeight), int32_t(curDepth)}},
                };

                // do the blit
                vkCmdBlitImage(
                    cmd,
                    tex->image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    tex->image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &blit,
                    VK_FILTER_LINEAR);

                // prepare current level to be transfer src for next mip
                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = texture,
                            // TODO: need better way to determine initial state, pass as parameter?
                            .stateBefore = ResourceState::TransferDst,
                            .stateAfter = ResourceState::TransferSrc,
                            .mipLevel = i,
                            .arrayLength = int32_t(tex->descriptor.arrayLength),
                        }),
                    });
            }

            // after the loop, transfer all mips to SHADER_READ_OPTIMAL
            submitBarriers(
                cmd,
                {
                    Barrier::from(Barrier::Image{
                        .texture = texture,
                        // TODO: need better way to determine initial state, pass as parameter?
                        .stateBefore = ResourceState::TransferSrc,
                        .stateAfter = ResourceState::SampleSource,
                        .mipLevel = 0,
                        .mipCount = tex->descriptor.mipLevels,
                        .arrayLength = int32_t(tex->descriptor.arrayLength),
                    }),
                });
        });
}