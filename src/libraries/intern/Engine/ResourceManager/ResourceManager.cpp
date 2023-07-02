#include "ResourceManager.hpp"
#include "Graphics/Buffer/Buffer.hpp"
#include "Graphics/Renderer/VulkanRenderer.hpp"
#include <Engine/Engine.hpp>
#include <TinyOBJ/tiny_obj_loader.h>
#include <vulkan/vulkan_core.h>

/*
    each "type implementation" is in the respective .cpp file
    ie: createBuffer in Buffer.cpp, createTexture in Texture.cpp ...
*/

void ResourceManager::init()
{
    ptr = this;
}

void ResourceManager::cleanup()
{
    for(int i = 0; i < freeSamplerIndex; i++)
    {
        VulkanRenderer::get()->deleteQueue.pushBack(
            [=]() { vkDestroySampler(VulkanRenderer::get()->device, samplerArray[i].sampler, nullptr); });
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