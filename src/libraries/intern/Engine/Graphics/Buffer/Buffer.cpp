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

    Handle<Buffer> newBufferHandle = bufferPool.insert(Buffer{
        .descriptor = {
            .size = createInfo.size,
            .memoryType = createInfo.memoryType,
            .allStates = createInfo.allStates,
        }});

    // if we can guarantee that no two threads access the pools at the same time we can
    // assume that the underlying pointer wont change until this function returns
    Buffer* buffer = bufferPool.get(newBufferHandle);

    VulkanDevice& gfxDevice = *VulkanDevice::get();

    // TODO: dont error, just warn and automatically set transfer_dst_bit
    assert(
        createInfo.initialData.empty() ||
        (createInfo.allStates & ResourceState::TransferDst &&
         "Attempting to create a buffer with inital content that does not have transfer destination bit set!"));

    VkBufferCreateInfo bufferCrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,

        .size = createInfo.size,
        .usage = toVkBufferUsage(createInfo.allStates),
    };

    VmaAllocationCreateInfo vmaAllocCrInfo = toVMAAllocCrInfo(createInfo.memoryType);
    VmaAllocator& allocator = gfxDevice.allocator;
    VkResult res = vmaCreateBuffer(
        allocator, &bufferCrInfo, &vmaAllocCrInfo, &buffer->buffer, &buffer->allocation, &buffer->allocInfo);

    assert(res == VK_SUCCESS);

    if(createInfo.memoryType == Buffer::MemoryType::CPU ||
       createInfo.memoryType == Buffer::MemoryType::GPU_BUT_CPU_VISIBLE)
        buffer->ptr = buffer->allocInfo.pMappedData;

    if(!createInfo.initialData.empty())
    {
        /*
            todo:
                - Currently waits for GPU Idle after upload
                - try asynchronous transfer queue
                - dont re-create staging buffers all the time!
                    keep a few large ones around?
                    or at least keep the last created ones around for a few frames in case theyre needed again?
        */

        // allocate staging buffer
        VkBufferCreateInfo stagingBufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = createInfo.size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        VmaAllocationCreateInfo vmaallocCrInfo = {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        AllocatedBuffer stagingBuffer;
        VkResult res = vmaCreateBuffer(
            gfxDevice.allocator,
            &stagingBufferInfo,
            &vmaallocCrInfo,
            &stagingBuffer.buffer,
            &stagingBuffer.allocation,
            &stagingBuffer.allocInfo);
        assert(res == VK_SUCCESS);

        memcpy(stagingBuffer.allocInfo.pMappedData, createInfo.initialData.data(), createInfo.initialData.size());

        gfxDevice.immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                VkBufferCopy copy{
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = createInfo.size,
                };
                vkCmdCopyBuffer(cmd, stagingBuffer.buffer, buffer->buffer, 1, &copy);
            });

        // since immediateSubmit also waits until the commands have executed, we can safely delete the staging
        // buffer immediately here
        //  todo: again, dont like the wait here. So once I fix that this call also has to be changed
        vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }

    if(!createInfo.debugName.empty())
    {
        gfxDevice.setDebugName(buffer->buffer, createInfo.debugName.data());
    }

    if((createInfo.allStates & ResourceState::UniformBuffer) ||
       (createInfo.allStates & ResourceState::UniformBufferGraphics) ||
       (createInfo.allStates & ResourceState::UniformBufferCompute))
    {
        buffer->resourceIndex =
            gfxDevice.bindlessManager.createBufferBinding(buffer->buffer, BindlessManager::BufferUsage::Uniform);
    }
    else if(
        (createInfo.allStates & ResourceState::Storage) ||
        (createInfo.allStates & ResourceState::StorageGraphics) ||
        (createInfo.allStates & ResourceState::StorageCompute))
    {
        buffer->resourceIndex =
            gfxDevice.bindlessManager.createBufferBinding(buffer->buffer, BindlessManager::BufferUsage::Storage);
    }

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
    const Buffer* buffer = bufferPool.get(handle);
    // its possible that this handle is outdated
    //   eg: if this buffer belonged to a mesh which has already been deleted (and during that deleted its
    //   buffers)
    if(buffer == nullptr)
    {
        return;
    }
    VulkanDevice& gfxDevice = *VulkanDevice::get();
    const VmaAllocator* allocator = &gfxDevice.allocator;
    const VkBuffer vkBuffer = buffer->buffer;
    const VmaAllocation vmaAllocation = buffer->allocation;
    gfxDevice.deleteQueue.pushBack([=]() { vmaDestroyBuffer(*allocator, vkBuffer, vmaAllocation); });
    bufferPool.remove(handle);
}