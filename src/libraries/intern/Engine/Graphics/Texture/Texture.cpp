#include "Texture.hpp"
#include "Graphics/Device/VulkanDevice.hpp"
#include "Graphics/Texture/Texture.hpp"
#include <Engine/Application/Application.hpp>
#include <Engine/Graphics/Barrier/Barrier.hpp>
#include <Engine/Graphics/Device/VulkanConversions.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <format>
#include <iostream>
#include <stb/stb_image.h>
#include <vulkan/vulkan_core.h>


Handle<Texture> ResourceManager::createTexture(Texture::LoadInfo&& loadInfo)
{
    auto* device = VulkanDevice::get();

    auto lastDirSep = loadInfo.path.find_last_of("/\\");
    auto extensionStart = loadInfo.path.find_last_of('.');
    std::string_view texName = loadInfo.debugName.empty()
                                   ? loadInfo.path.substr(lastDirSep + 1, extensionStart - lastDirSep - 1)
                                   : loadInfo.debugName;

    loadInfo.debugName = texName;

    // todo: handle naming collisions
    auto iterator = nameToMeshLUT.find(texName);
    assert(iterator == nameToMeshLUT.end());

    std::string_view extension = loadInfo.path.substr(extensionStart);

    // TODO: LOG: warn if not alraedy set
    loadInfo.allStates |= ResourceState::TransferDst;
    if(loadInfo.fillMipLevels && (loadInfo.mipLevels == Texture::MipLevels::All || loadInfo.mipLevels > 1))
    {
        // TODO: LOG: warn if not already set
        loadInfo.allStates |= ResourceState::TransferSrc; // needed for blit source :/
    }

    Texture::CreateInfo createInfo;
    std::function<void()> cleanupFunc;
    if(extension == ".hdr")
    {
        std::tie(createInfo, cleanupFunc) = Texture::loadHDR(std::move(loadInfo));
    }
    else
    {
        std::tie(createInfo, cleanupFunc) = Texture::loadDefault(std::move(loadInfo));
    }

    Handle<Texture> newTextureHandle = createTexture(std::move(createInfo));

    cleanupFunc();

    return newTextureHandle;
}

Handle<Texture> ResourceManager::createTexture(Texture::CreateInfo&& createInfo)
{
    auto* device = VulkanDevice::get();

    // todo: handle naming collisions and also dont just use a random name...
    std::string name = createInfo.debugName;
    if(name.empty())
    {
        name = "Texture" + std::format("{:03}", rand() % 199);
    }
    auto iterator = nameToMeshLUT.find(name);
    assert(iterator == nameToMeshLUT.end());

    const uint32_t maxDimension = std::max({createInfo.size.width, createInfo.size.height, createInfo.size.depth});
    int32_t actualMipLevels = createInfo.mipLevels == Texture::MipLevels::All
                                  ? int32_t(floor(log2(maxDimension))) + 1
                                  : createInfo.mipLevels;
    assert(actualMipLevels > 0);
    createInfo.mipLevels = actualMipLevels;

    if(!createInfo.initialData.empty())
    {
        // TODO: LOG: warn
        createInfo.allStates |= ResourceState::TransferDst;
    }

    if(createInfo.mipLevels == Texture::MipLevels::All || createInfo.mipLevels > 1)
    {
        // TODO: LOG: warn
        createInfo.allStates |= ResourceState::TransferSrc;
        createInfo.allStates |= ResourceState::TransferDst;
    }

    Handle<Texture> newTextureHandle = texturePool.insert(Texture{
        .descriptor = {
            .type = createInfo.type,
            .format = createInfo.format,
            .allStates = createInfo.allStates,
            .size = createInfo.size,
            .mipLevels = createInfo.mipLevels,
            .arrayLength = createInfo.arrayLength,
        }});
    Texture* tex = texturePool.get(newTextureHandle);
    assert(tex);

    VkImageCreateFlags flags = createInfo.type == Texture::Type::tCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

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
        .arrayLayers = toVkArrayLayers(tex->descriptor),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = toVkImageUsage(createInfo.allStates),
    };
    VmaAllocationCreateInfo imgAllocInfo{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    VmaAllocator& allocator = device->allocator;
    VkResult res = vmaCreateImage(allocator, &imageCrInfo, &imgAllocInfo, &tex->image, &tex->allocation, nullptr);
    assert(res == VK_SUCCESS);

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
            device->allocator,
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

        device->immediateSubmit(
            [&](VkCommandBuffer cmd)
            {
                device->insertBarriers(
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

                device->insertBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = newTextureHandle,
                            .stateBefore = ResourceState::TransferDst,
                            .stateAfter = createInfo.initialState,
                        }),
                    });

                if(createInfo.fillMipLevels && createInfo.mipLevels > 1)
                {
                    device->fillMipLevels(cmd, newTextureHandle, createInfo.initialState);
                }
            });

        // immediate submit also waits on device idle so staging buffer is not being used here anymore
        vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }
    else if(createInfo.initialState != ResourceState::Undefined)
    {
        // Dont upload anything, just transition
        device->immediateSubmit(
            [&](VkCommandBuffer cmd)
            {
                device->insertBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = newTextureHandle,
                            .stateBefore = ResourceState::Undefined,
                            .stateAfter = createInfo.initialState,
                        }),
                    });
            });
    }

    nameToTextureLUT.insert({std::string{name}, newTextureHandle});

    device->setDebugName(tex->image, (std::string{name} + "_image").c_str());

    // Creating the resourceViews and descriptors for bindless

    assert(actualMipLevels > 0);
    tex->_mipResourceIndices = std::make_unique<uint32_t[]>(actualMipLevels);
    for(int i = 0; i < actualMipLevels; i++)
        tex->_mipResourceIndices[i] = 0xFFFFFFFF;
    tex->_mipImageViews = std::make_unique<VkImageView[]>(actualMipLevels);
    for(int i = 0; i < actualMipLevels; i++)
        tex->_mipImageViews[i] = VK_NULL_HANDLE;

    if(actualMipLevels == 1)
    {
        // create a single view and single resourceIndex
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
                    .layerCount = toVkArrayLayers(tex->descriptor),
                },
        };

        vkCreateImageView(device->device, &imageViewCrInfo, nullptr, &tex->_fullImageView);
        tex->_mipImageViews[0] = tex->_fullImageView;
        device->setDebugName(tex->_fullImageView, (std::string{name} + "_viewFull").c_str());

        // TODO: containsSamplingState()/...StorageState() functions!
        bool usedForSampling = (createInfo.allStates & ResourceState::SampleSource) ||
                               (createInfo.allStates & ResourceState::SampleSourceGraphics) ||
                               (createInfo.allStates & ResourceState::SampleSourceCompute);
        bool usedForStorage = (createInfo.allStates & ResourceState::Storage) ||
                              (createInfo.allStates & ResourceState::StorageRead) ||
                              (createInfo.allStates & ResourceState::StorageWrite) ||
                              (createInfo.allStates & ResourceState::StorageGraphics) ||
                              (createInfo.allStates & ResourceState::StorageGraphicsRead) ||
                              (createInfo.allStates & ResourceState::StorageGraphicsWrite) ||
                              (createInfo.allStates & ResourceState::StorageCompute) ||
                              (createInfo.allStates & ResourceState::StorageComputeRead) ||
                              (createInfo.allStates & ResourceState::StorageComputeWrite);

        if(usedForSampling && usedForStorage)
        {
            tex->_fullResourceIndex =
                device->bindlessManager.createImageBinding(tex->_fullImageView, BindlessManager::ImageUsage::Both);
        }
        // else: not both but maybe one of the two
        else if(usedForSampling)
        {
            tex->_fullResourceIndex = device->bindlessManager.createImageBinding(
                tex->_fullImageView, BindlessManager::ImageUsage::Sampled);
        }
        else if(usedForStorage)
        {
            tex->_fullResourceIndex = device->bindlessManager.createImageBinding(
                tex->_fullImageView, BindlessManager::ImageUsage::Storage);
        }
        tex->_mipResourceIndices[0] = tex->_fullResourceIndex;
    }
    else
    {
        // create a view for the full image, can not be used for storage!
        {
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
                        .layerCount = toVkArrayLayers(tex->descriptor),
                    },
            };

            vkCreateImageView(device->device, &imageViewCrInfo, nullptr, &tex->_fullImageView);
            device->setDebugName(tex->_fullImageView, (std::string{name} + "_viewFull").c_str());

            bool usedForSampling = (createInfo.allStates & ResourceState::SampleSource) ||
                                   (createInfo.allStates & ResourceState::SampleSourceGraphics) ||
                                   (createInfo.allStates & ResourceState::SampleSourceCompute);

            if(usedForSampling)
            {
                tex->_fullResourceIndex = device->bindlessManager.createImageBinding(
                    tex->_fullImageView, BindlessManager::ImageUsage::Sampled);
            }
        }

        // Create views for all individual mips, storage only for now, but wouldnt be hard to change
        for(int m = 0; m < actualMipLevels; m++)
        {
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
                        .baseMipLevel = static_cast<uint32_t>(m),
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = toVkArrayLayers(tex->descriptor),
                    },
            };

            vkCreateImageView(device->device, &imageViewCrInfo, nullptr, &tex->_mipImageViews[m]);
            device->setDebugName(
                tex->_mipImageViews[m], (std::string{name} + "_viewMip" + std::to_string(m)).c_str());

            VulkanDevice& gfxDevice = *VulkanDevice::get();
            bool usedForSampling = (createInfo.allStates & ResourceState::SampleSource) ||
                                   (createInfo.allStates & ResourceState::SampleSourceGraphics) ||
                                   (createInfo.allStates & ResourceState::SampleSourceCompute);
            bool usedForStorage = (createInfo.allStates & ResourceState::Storage) ||
                                  (createInfo.allStates & ResourceState::StorageRead) ||
                                  (createInfo.allStates & ResourceState::StorageWrite) ||
                                  (createInfo.allStates & ResourceState::StorageGraphics) ||
                                  (createInfo.allStates & ResourceState::StorageGraphicsRead) ||
                                  (createInfo.allStates & ResourceState::StorageGraphicsWrite) ||
                                  (createInfo.allStates & ResourceState::StorageCompute) ||
                                  (createInfo.allStates & ResourceState::StorageComputeRead) ||
                                  (createInfo.allStates & ResourceState::StorageComputeWrite);

            // if(usedForSampling && usedForStorage)
            // {
            //     tex->_fullResourceIndex = gfxDevice.bindlessManager.createImageBinding(
            //         tex->_fullImageView, BindlessManager::ImageUsage::Both);
            // }
            // // else: not both but maybe one of the two
            // else if(usedForSampling)
            // {
            //     tex->_fullResourceIndex = gfxDevice.bindlessManager.createImageBinding(
            //         tex->_fullImageView, BindlessManager::ImageUsage::Sampled);
            // }
            // else if(usedForStorage)
            if(usedForStorage)
            {
                tex->_mipResourceIndices[m] = gfxDevice.bindlessManager.createImageBinding(
                    tex->_mipImageViews[m], BindlessManager::ImageUsage::Storage);
            }
        }
    }

    return newTextureHandle;
}

void ResourceManager::free(Handle<Texture> handle)
{
    auto* gfxDevice = VulkanDevice::get();

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
    const VmaAllocator* allocator = &gfxDevice->allocator;
    VkImage image = texture->image;
    VmaAllocation vmaAllocation = texture->allocation;
    VkDevice vkdevice = gfxDevice->device;
    gfxDevice->deleteQueue.pushBack([=]() { vmaDestroyImage(gfxDevice->allocator, image, vmaAllocation); });

    VkImageView imageView = texture->_fullImageView;
    gfxDevice->deleteQueue.pushBack([=]() { vkDestroyImageView(vkdevice, imageView, nullptr); });
    if(texture->descriptor.mipLevels > 1)
        for(int i = 0; i < texture->descriptor.mipLevels; i++)
        {
            imageView = texture->_mipImageViews[i];
            gfxDevice->deleteQueue.pushBack([=]() { vkDestroyImageView(vkdevice, imageView, nullptr); });
        }

    texturePool.remove(handle);
}

uint32_t Texture::fullResourceIndex() const
{
    return _fullResourceIndex;
}
/*
    WARNING: ATM ALL SINGLE MIPS ONLY GET STORAGE RESOURCES!
    IF SINGLE MIPS ARE NEEDED FOR SAMPLED RESOURCES, USE fullResouceIndex() INSTEAD!
*/
uint32_t Texture::mipResourceIndex(uint32_t level) const
{
    return _mipResourceIndices[level];
}

// TODO: own textureView abstraction
VkImageView Texture::fullResourceView() const
{
    return _fullImageView;
}
VkImageView Texture::mipResourceView(uint32_t level) const
{
    return _mipImageViews[level];
}