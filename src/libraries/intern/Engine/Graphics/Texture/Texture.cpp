#include "Texture.hpp"
#include "Graphics/Renderer/VulkanRenderer.hpp"
#include "Graphics/Texture/Texture.hpp"
#include "IO/HDR.hpp"
#include <Engine/Engine.hpp>
#include <Engine/Graphics/Renderer/VulkanDebug.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <format>
#include <iostream>
#include <stb/stb_image.h>
#include <vulkan/vulkan_core.h>

Handle<Texture>
ResourceManager::createTexture(const char* file, VkImageUsageFlags usage, bool dataIsLinear, std::string_view name)
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
    VkFormat imageFormat = dataIsLinear ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
    VkExtent3D imageExtent{
        .width = (uint32_t)texWidth,
        .height = (uint32_t)texHeight,
        .depth = 1,
    };

    Handle<Texture> newTextureHandle = createTexture(Texture::Info{
        // specifying non-default values only
        .debugName = std::string{texName},
        .size = imageExtent,
        .format = imageFormat,
        .usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialData = {pixels, imageSize},
    });

    stbi_image_free(pixels);

    return newTextureHandle;
}

Handle<Texture> ResourceManager::createTexture(Texture::Info&& info)
{
    // todo: handle naming collisions and also dont just use a random name...
    std::string name = info.debugName;
    if(name.empty())
    {
        name = "Texture" + std::format("{:03}", rand() % 199);
    }
    auto iterator = nameToMeshLUT.find(name);
    assert(iterator == nameToMeshLUT.end());

    Handle<Texture> newTextureHandle = texturePool.insert(Texture{.info = info});
    Texture* tex = texturePool.get(newTextureHandle);
    assert(tex);

    VkImageCreateInfo imageCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = info.flags,

        .imageType = info.imageType,
        .format = info.format,
        .extent = info.size,

        .mipLevels = info.mipLevels,
        .arrayLayers = info.arrayLayers,
        .samples = info.samples,
        .tiling = info.tiling,
        .usage = info.usage,
    };
    VmaAllocationCreateInfo imgAllocInfo{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    VmaAllocator& allocator = Engine::get()->getRenderer()->allocator;
    vmaCreateImage(allocator, &imageCrInfo, &imgAllocInfo, &tex->image, &tex->allocation, nullptr);

    if(!info.initialData.empty())
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
            .size = info.initialData.size(),
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
        memcpy(data, info.initialData.data(), info.initialData.size());
        vmaUnmapMemory(allocator, stagingBuffer.allocation);

        Engine::get()->getRenderer()->immediateSubmit(
            [&](VkCommandBuffer cmd)
            {
                VkImageSubresourceRange range{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                };

                VkImageMemoryBarrier imageBarrierToTransfer{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = 0,
                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .image = tex->image,
                    .subresourceRange = range,
                };

                vkCmdPipelineBarrier(
                    cmd,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &imageBarrierToTransfer);

                VkBufferImageCopy copyRegion{
                    .bufferOffset = 0,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource =
                        {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                         .mipLevel = 0,
                         .baseArrayLayer = 0,
                         .layerCount = 1},
                    .imageExtent = info.size,
                };

                vkCmdCopyBufferToImage(
                    cmd, stagingBuffer.buffer, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                VkImageMemoryBarrier imageBarrierToReadable{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .image = tex->image,
                    .subresourceRange = range,
                };

                vkCmdPipelineBarrier(
                    cmd,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &imageBarrierToReadable);
            });

        // immediate submit also waits on device idle so staging buffer is not being used here anymore
        vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }

    nameToTextureLUT.insert({std::string{name}, newTextureHandle});

    // create an image view covering the whole image
    VkImageViewType viewType =
        info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    VkImageViewCreateInfo imageViewCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = tex->image,
        .viewType = viewType,
        .format = info.format,
        .components =
            VkComponentMapping{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = info.viewAspect,
                .baseMipLevel = 0,
                .levelCount = info.mipLevels,
                .baseArrayLayer = 0,
                .layerCount = info.arrayLayers,
            },
    };

    vkCreateImageView(Engine::get()->getRenderer()->device, &imageViewCrInfo, nullptr, &tex->imageView);

    setDebugName(tex->image, (std::string{name} + "_image").c_str());
    setDebugName(tex->imageView, (std::string{name} + "_mainView").c_str());

    VulkanRenderer& renderer = *VulkanRenderer::get();
    if(tex->info.usage & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        tex->sampledResourceIndex = renderer.bindlessManager.createSampledImageBinding(
            tex->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    if(tex->info.usage & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        tex->storageResourceIndex =
            renderer.bindlessManager.createStorageImageBinding(tex->imageView, VK_IMAGE_LAYOUT_GENERAL);
    }

    return newTextureHandle;
}

Handle<Texture> ResourceManager::createCubemapFromEquirectangular(
    uint32_t cubeResolution, Handle<Texture> equirectangularSource, std::string_view debugName)
{
    auto iterator = nameToMeshLUT.find(debugName);
    assert(iterator == nameToMeshLUT.end());

    Texture* sourceTex = get(equirectangularSource);

    Handle<Texture> newTextureHandle = createTexture(Texture::Info{
        // specifying non-default values only
        .debugName = std::string{debugName},
        .size = {.width = cubeResolution, .height = cubeResolution, .depth = 1},
        .format = sourceTex->info.format,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .arrayLayers = 6,
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
    Texture* cubeTex = get(newTextureHandle);

    // TODO: !!!! DONT CREATE HERE !!! CREATE ON INIT() OF ENGINE AND JUST GET BY NAME OR SO HERE !
    // Handle<ComputeShader> conversionShaderHandle =
    //     createComputeShader(SHADERS_PATH "/equiToCube.comp", "equiToCubeCompute");
    Handle<ComputeShader> conversionShaderHandle = createComputeShader(
        {
            .sourcePath = SHADERS_PATH "/equiToCube.hlsl",
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
        .sourceIndex = sourceTex->sampledResourceIndex,
        .samplerIndex = get(linearSampler)->resourceIndex,
        .targetIndex = cubeTex->storageResourceIndex,
    };
    renderer->immediateSubmit(
        [=](VkCommandBuffer cmd)
        {
            // TODO: THE CURRENT SYNCHRONIZATION IS BY NO MEANS OPTIMAL !
            //       but could at least switch to synch2

            VkImageSubresourceRange range{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 6,
            };

            VkImageMemoryBarrier imageBarrierToTransfer{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .image = cubeTex->image,
                .subresourceRange = range,
            };

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &imageBarrierToTransfer);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, conversionShader->pipeline);
            // Bind the bindless descriptor sets once per cmdbuffer
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                renderer->bindlessPipelineLayout,
                0,
                4,
                renderer->bindlessManager.bindlessDescriptorSets.data(),
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

            VkImageMemoryBarrier imageBarrierToReadable{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image = cubeTex->image,
                .subresourceRange = range,
            };

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &imageBarrierToReadable);
        });

    return newTextureHandle;
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