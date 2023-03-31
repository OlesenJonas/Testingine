#include "Texture.hpp"
#include <intern/Engine/Engine.hpp>
#include <intern/ResourceManager/ResourceManager.hpp>
#include <iostream>
#include <stb/stb_image.h>

Handle<Texture> ResourceManager::createTexture(const char* file, VkImageUsageFlags usage, std::string_view name)
{
    std::string_view fileView{file};
    auto lastDirSep = fileView.find_last_of("/\\");
    auto extensionStart = fileView.find_last_of('.');
    std::string_view texName =
        name.empty() ? fileView.substr(lastDirSep + 1, extensionStart - lastDirSep - 1) : name;

    // todo: handle naming collisions
    auto iterator = nameToMeshLUT.find(texName);
    assert(iterator == nameToMeshLUT.end());

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
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
    VkExtent3D imageExtent{
        .width = (uint32_t)texWidth,
        .height = (uint32_t)texHeight,
        .depth = 1,
    };

    Handle<Texture> newTextureHandle = texturePool.insert(Texture{
        .info =
            {
                .size = imageExtent,
                .usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            },
    });
    Texture* tex = texturePool.get(newTextureHandle);

    VkImageCreateInfo imageCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,

        .imageType = VK_IMAGE_TYPE_2D,
        .format = imageFormat,
        .extent = imageExtent,

        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    VmaAllocationCreateInfo imgAllocInfo{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    VmaAllocator& allocator = Engine::get()->getRenderer()->allocator;
    vmaCreateImage(allocator, &imageCrInfo, &imgAllocInfo, &tex->image, &tex->allocation, nullptr);

    // Upload the pixel buffer through staging buffer, see the createBuffer(...) function for todos on how to
    // improve this
    // allocate staging buffer
    VkBufferCreateInfo stagingBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = imageSize,
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
    memcpy(data, pixelPtr, imageSize);
    vmaUnmapMemory(allocator, stagingBuffer.allocation);

    stbi_image_free(pixels);

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
                    {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                .imageExtent = imageExtent,
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

    nameToTextureLUT.insert({std::string{texName}, newTextureHandle});

    // create an image view covering the whole image
    VkImageViewCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = tex->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCreateImageView(Engine::get()->getRenderer()->device, &imageInfo, nullptr, &tex->imageView);

    std::cout << "Texture loaded successfully " << file << std::endl;

    return newTextureHandle;
}

void ResourceManager::deleteTexture(Handle<Texture> handle)
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