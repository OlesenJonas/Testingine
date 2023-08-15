#include "MaterialInstance.hpp"

#include <Datastructures/ArrayHelpers.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>

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

        for(int i = 0; i < VulkanDevice::FRAMES_IN_FLIGHT; i++)
        {
            matInst->parameters.gpuBuffers[i] = createBuffer(Buffer::CreateInfo{
                .size = matInst->parameters.bufferSize,
                .memoryType = Buffer::MemoryType::GPU_BUT_CPU_VISIBLE,
                .allStates = ResourceState::UniformBuffer | ResourceState::TransferDst,
                .initialState = ResourceState::UniformBuffer,
                .initialData = {(uint8_t*)matInst->parameters.cpuBuffer, matInst->parameters.bufferSize},
            });
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

    VulkanDevice& gfxDevice = *VulkanDevice::get();
    VkDevice device = gfxDevice.device;
    const VmaAllocator* allocator = &gfxDevice.allocator;

    if(matInst->parameters.cpuBuffer != nullptr)
    {
        ::free(matInst->parameters.cpuBuffer);

        uint32_t bufferCount = ArraySize(matInst->parameters.gpuBuffers);
        for(int i = 0; i < bufferCount; i++)
        {
            Handle<Buffer> bufferHandle = matInst->parameters.gpuBuffers[i];
            Buffer* buffer = get(bufferHandle);
            if(buffer == nullptr)
            {
                return;
            }
            gfxDevice.deleteBuffer(buffer);
            bufferPool.remove(bufferHandle);
        }
    }

    materialInstancePool.remove(handle);
}