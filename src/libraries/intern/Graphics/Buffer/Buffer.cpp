#include "Buffer.hpp"
#include <intern/Graphics/Renderer/VulkanDebug.hpp>
#include <intern/Graphics/Renderer/VulkanRenderer.hpp>
#include <intern/ResourceManager/ResourceManager.hpp>
#include <vulkan/vulkan_core.h>

Handle<Buffer> ResourceManager::createBuffer(Buffer::CreateInfo crInfo, std::string_view name)
{
    Handle<Buffer> newBufferHandle = bufferPool.insert(Buffer{.info = crInfo.info});

    // if we can guarantee that no two threads access the pools at the same time we can
    // assume that the underlying pointer wont change until this function returns
    Buffer* buffer = bufferPool.get(newBufferHandle);

    VulkanRenderer& renderer = *VulkanRenderer::get();

    // TODO: dont error, just warn and automatically set transfer_dst_bit
    assert(
        crInfo.initialData == nullptr ||
        (crInfo.info.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) > 0 &&
            "Attempting to create a buffer with inital content that does not have transfer destination bit set!");

    VkBufferCreateInfo bufferCrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,

        .size = crInfo.info.size,
        .usage = crInfo.info.usage,
    };

    VmaAllocationCreateInfo vmaAllocCrInfo{
        .flags = crInfo.info.memoryAllocationInfo.flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = crInfo.info.memoryAllocationInfo.requiredMemoryPropertyFlags,
    };

    VmaAllocator& allocator = renderer.allocator;
    VkResult res = vmaCreateBuffer(
        allocator, &bufferCrInfo, &vmaAllocCrInfo, &buffer->buffer, &buffer->allocation, &buffer->allocInfo);

    assert(res == VK_SUCCESS);

    if(crInfo.initialData != nullptr)
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
            .size = crInfo.info.size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        VmaAllocationCreateInfo vmaallocCrInfo = {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        AllocatedBuffer stagingBuffer;
        VkResult res = vmaCreateBuffer(
            renderer.allocator,
            &stagingBufferInfo,
            &vmaallocCrInfo,
            &stagingBuffer.buffer,
            &stagingBuffer.allocation,
            &stagingBuffer.allocInfo);
        assert(res == VK_SUCCESS);

        void* data = nullptr;
        vmaMapMemory(allocator, stagingBuffer.allocation, &data);
        memcpy(data, crInfo.initialData, crInfo.info.size);
        vmaUnmapMemory(allocator, stagingBuffer.allocation);

        renderer.immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                VkBufferCopy copy{
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = crInfo.info.size,
                };
                vkCmdCopyBuffer(cmd, stagingBuffer.buffer, buffer->buffer, 1, &copy);
            });

        // since immediateSubmit also waits until the commands have executed, we can safely delete the staging
        // buffer immediately here
        //  todo: again, dont like the wait here. So once I fix that this call also has to be changed
        vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }

    if(!name.empty())
    {
        setDebugName(buffer->buffer, name.data());
    }

    if(crInfo.info.usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    {
        buffer->uniformResourceIndex = renderer.createUniformBufferBinding(buffer->buffer);
    }
    if(crInfo.info.usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
    {
        buffer->storageResourceIndex = renderer.createStorageBufferBinding(buffer->buffer);
    }

    return newBufferHandle;
}

void ResourceManager::deleteBuffer(Handle<Buffer> handle)
{
    /*
        todo:
        Enqueue into a per frame queue
        also need to ensure renderer deletes !all! objects from per frame queue during shutdown, ignoring the
            current frame!
    */
    const Buffer* buffer = bufferPool.get(handle);
    // its possible that this handle is outdated
    //   eg: if this buffer belonged to a mesh which has already been deleted (and during that deleted its buffers)
    if(buffer == nullptr)
    {
        return;
    }
    VulkanRenderer& renderer = *VulkanRenderer::get();
    const VmaAllocator* allocator = &renderer.allocator;
    const VkBuffer vkBuffer = buffer->buffer;
    const VmaAllocation vmaAllocation = buffer->allocation;
    renderer.deleteQueue.pushBack([=]() { vmaDestroyBuffer(*allocator, vkBuffer, vmaAllocation); });
    bufferPool.remove(handle);
}