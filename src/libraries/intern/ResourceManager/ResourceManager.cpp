#include "ResourceManager.hpp"
#include "Graphics/Buffer/Buffer.hpp"
#include "Graphics/Renderer/VulkanRenderer.hpp"
#include <TinyOBJ/tiny_obj_loader.h>
#include <intern/Engine/Engine.hpp>
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
    // I dont like this way of "clearing" the pools, but works for now...

    for(int i = 0; i < freeSamplerIndex; i++)
    {
        VulkanRenderer::get()->deleteQueue.pushBack(
            [=]() { vkDestroySampler(VulkanRenderer::get()->device, samplerArray[i].sampler, nullptr); });
    }

    Handle<Texture> texHandle = texturePool.getFirst();
    while(texHandle.isValid())
    {
        deleteTexture(texHandle);
        texHandle = texturePool.getFirst();
    }

    Handle<Buffer> bufferHandle = bufferPool.getFirst();
    while(bufferHandle.isValid())
    {
        deleteBuffer(bufferHandle);
        bufferHandle = bufferPool.getFirst();
    }

    Handle<Mesh> meshHandle = meshPool.getFirst();
    while(meshHandle.isValid())
    {
        deleteMesh(meshHandle);
        meshHandle = meshPool.getFirst();
    }

    Handle<Material> materialHandle = materialPool.getFirst();
    while(materialHandle.isValid())
    {
        free(materialHandle);
        materialHandle = materialPool.getFirst();
    }
}