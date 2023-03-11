#include "Texture.hpp"
#include "VulkanRenderer.hpp"

#include <iostream>
#include <stb/stb_image.h>
#include <vulkan/vulkan_core.h>

bool loadImageFromFile(VulkanRenderer& renderer, const char* file, AllocatedImage& image)
{
    int texWidth = 0;
    int texHeight = 0;
    int texChannels = 0;

    stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if(pixels == nullptr)
    {
        std::cout << "Failed to load texture file: " << file << std::endl;
        return false;
    }

    void* pixelPtr = pixels;
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

    AllocatedBuffer stagingBuffer =
        renderer.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data;
    vmaMapMemory(renderer.allocator, stagingBuffer.allocation, &data);
    memcpy(data, pixelPtr, static_cast<size_t>(imageSize));
    vmaUnmapMemory(renderer.allocator, stagingBuffer.allocation);

    stbi_image_free(pixels);

    VkExtent3D imageExtent{
        .width = (uint32_t)texWidth,
        .height = (uint32_t)texHeight,
        .depth = 1,
    };

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
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    AllocatedImage newImage;
    VmaAllocationCreateInfo imgAllocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };
    vmaCreateImage(
        renderer.allocator, &imageCrInfo, &imgAllocInfo, &newImage.image, &newImage.allocation, nullptr);

    renderer.immediateSubmit(
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
                .image = newImage.image,
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
                cmd, stagingBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            VkImageMemoryBarrier imageBarrierToReadable{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image = newImage.image,
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

    renderer.deleteQueue.pushBack([=]()
                                  { vmaDestroyImage(renderer.allocator, newImage.image, newImage.allocation); });

    // immediate submit also waits on device idle so staging buffer is not being used here anymore
    vmaDestroyBuffer(renderer.allocator, stagingBuffer.buffer, stagingBuffer.allocation);

    std::cout << "Texture loaded successfully " << file << std::endl;

    image = newImage;

    return true;
}