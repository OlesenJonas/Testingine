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
    const uint32_t maxMipLevels = int32_t(floor(log2(maxDimension))) + 1;
    // TODO: warn if clmaping to max happened
    int32_t actualMipLevels = createInfo.mipLevels == Texture::MipLevels::All
                                  ? maxMipLevels
                                  : std::min<int32_t>(createInfo.mipLevels, maxMipLevels);
    assert(actualMipLevels > 0);
    createInfo.mipLevels = actualMipLevels;

    if(!createInfo.initialData.empty())
    {
        // TODO: LOG: warn
        createInfo.allStates |= ResourceState::TransferDst;
    }

    if(createInfo.mipLevels > 1)
    {
        // TODO: LOG: warn (needed for automatic mip fill)
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

    tex->gpuTexture = device->createTexture(createInfo);

    nameToTextureLUT.insert({std::string{name}, newTextureHandle});

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
    Texture* texture = texturePool.get(handle);
    if(texture == nullptr)
    {
        return;
    }
    gfxDevice->deleteTexture(texture);

    texturePool.remove(handle);
}

uint32_t Texture::fullResourceIndex() const
{
    return gpuTexture.resourceIndex;
}
/*
    WARNING: ATM ALL SINGLE MIPS ONLY GET STORAGE RESOURCES!
    IF SINGLE MIPS ARE NEEDED FOR SAMPLED RESOURCES, USE fullResouceIndex() INSTEAD!
*/
uint32_t Texture::mipResourceIndex(uint32_t level) const
{
    return gpuTexture._mipResourceIndices[level];
}

// TODO: own textureView abstraction
VkImageView Texture::fullResourceView() const
{
    return gpuTexture.fullImageView;
}
VkImageView Texture::mipResourceView(uint32_t level) const
{
    return gpuTexture._mipImageViews[level];
}