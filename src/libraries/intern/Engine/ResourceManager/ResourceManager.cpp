#include "ResourceManager.hpp"
#include "Graphics/Buffer/Buffer.hpp"
#include "Graphics/Device/VulkanDevice.hpp"
#include <Engine/Application/Application.hpp>
#include <TinyOBJ/tiny_obj_loader.h>
#include <vulkan/vulkan_core.h>

/*
    each "type implementation" is in the respective .cpp file
    ie: createBuffer in Buffer.cpp, createTexture in Texture.cpp ...
    //TODO: change?
*/

void ResourceManager::init()
{
    INIT_STATIC_GETTER();
    _initialized = true;
}

void ResourceManager::cleanup()
{
    auto* device = VulkanDevice::get();

    for(int i = 0; i < freeSamplerIndex; i++)
    {
        device->deleteQueue.pushBack([=]()
                                     { vkDestroySampler(device->device, samplerArray[i].sampler, nullptr); });
    }

    auto clearPool = [&](auto&& pool)
    {
        Handle handle = pool.getFirst();
        while(handle.isValid())
        {
            free(handle);
            handle = pool.getFirst();
        }
    };

    clearPool(computeShaderPool);
    clearPool(materialInstancePool);
    clearPool(materialPool);
    clearPool(texturePool);
    clearPool(bufferPool);
    clearPool(meshPool);
}