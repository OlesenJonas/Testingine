#include "Buffer.hpp"
#include <Engine/Graphics/Device/VulkanDevice.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <VMA/VMA.hpp>
#include <vulkan/vulkan_core.h>

Handle<Buffer> ResourceManager::createBuffer(Buffer::CreateInfo&& createInfo)
{
    if(createInfo.allStates.containsStorageBufferUsage() && createInfo.allStates.containsUniformBufferUsage())
    {
        // TODO: LOGGER: Cant use both, using just storage
        createInfo.allStates.unset(
            ResourceState::UniformBuffer | ResourceState::UniformBufferGraphics |
            ResourceState::UniformBufferCompute);
        assert(!createInfo.allStates.containsUniformBufferUsage());
    }
    if(!createInfo.initialData.empty() && !(createInfo.allStates & ResourceState::TransferDst))
    {
        // TODO: Log: Trying to create buffer with initial data but without transfer dst bit set
        createInfo.allStates |= ResourceState::TransferDst;
    }

    VulkanDevice& gfxDevice = *VulkanDevice::get();

    VulkanBuffer gpuBuffer = gfxDevice.createBuffer(createInfo);

    // TODO: lock pool?
    Handle<Buffer> newBufferHandle = bufferPool.insert(Buffer{
        .descriptor =
            {
                .size = createInfo.size,
                .memoryType = createInfo.memoryType,
                .allStates = createInfo.allStates,
            },
        .gpuBuffer = gpuBuffer,
    });

    return newBufferHandle;
}

void ResourceManager::free(Handle<Buffer> handle)
{
    /*
        todo:
        Enqueue into a per frame queue
        also need to ensure gfxDevice deletes !all! objects from per frame queue during shutdown, ignoring the
            current frame!
    */
    Buffer* buffer = bufferPool.get(handle);
    // its possible that this handle is outdated
    //   eg: if this buffer belonged to a mesh which has already been deleted (and during that deleted its
    //   buffers)
    if(buffer == nullptr)
    {
        return;
    }
    VulkanDevice* gfxDevice = VulkanDevice::get();
    gfxDevice->deleteBuffer(buffer);
    bufferPool.remove(handle);
}