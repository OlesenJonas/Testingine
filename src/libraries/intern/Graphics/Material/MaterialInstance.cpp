#include "MaterialInstance.hpp"

#include <intern/Datastructures/ArraySize.hpp>
#include <intern/ResourceManager/ResourceManager.hpp>

Handle<MaterialInstance> ResourceManager::createMaterialInstance(Handle<Material> material)
{
    Handle<MaterialInstance> matInstHandle = materialInstancePool.insert();
    MaterialInstance* matInst = get(matInstHandle);
    Material* parentMat = get(material);

    matInst->parentMaterial = material;
    matInst->parameters.bufferSize = parentMat->instanceParametersBufferSize;
    if(matInst->parameters.bufferSize > 0)
    {
        matInst->parameters.lut = parentMat->instanceParametersLUT;
        matInst->parameters.cpuBuffer = (char*)malloc(parentMat->instanceParametersBufferSize);
        memset(matInst->parameters.cpuBuffer, 0, matInst->parameters.bufferSize);

        for(int i = 0; i < VulkanRenderer::FRAMES_IN_FLIGHT; i++)
        {
            matInst->parameters.gpuBuffers[i] = createBuffer(Buffer::CreateInfo{
                .info =
                    {
                        .size = matInst->parameters.bufferSize,
                        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        .memoryAllocationInfo =
                            {
                                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                         VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            },
                    },
                .initialData = matInst->parameters.cpuBuffer});
        }
    }

    return matInstHandle;
}

void ResourceManager::free(Handle<MaterialInstance> handle)
{
    const MaterialInstance* matInst = materialInstancePool.get(handle);
    if(matInst == nullptr)
    {
        return;
    }

    VulkanRenderer& renderer = *VulkanRenderer::get();
    VkDevice device = renderer.device;
    const VmaAllocator* allocator = &renderer.allocator;

    if(matInst->parameters.cpuBuffer != nullptr)
    {
        ::free(matInst->parameters.cpuBuffer);

        uint32_t bufferCount = ArraySize(matInst->parameters.gpuBuffers);
        for(int i = 0; i < bufferCount; i++)
        {
            Handle<Buffer> bufferHandle = matInst->parameters.gpuBuffers[i];
            const Buffer* buffer = get(bufferHandle);
            if(buffer == nullptr)
            {
                return;
            }
            const VkBuffer vkBuffer = buffer->buffer;
            const VmaAllocation vmaAllocation = buffer->allocation;
            renderer.deleteQueue.pushBack([=]() { vmaDestroyBuffer(*allocator, vkBuffer, vmaAllocation); });
            bufferPool.remove(bufferHandle);
        }
    }

    materialInstancePool.remove(handle);
}