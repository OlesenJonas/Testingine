#include "ResourceManager.hpp"
#include "Graphics/Buffer/Buffer.hpp"
#include <TinyOBJ/tiny_obj_loader.h>
#include <intern/Engine/Engine.hpp>
#include <vulkan/vulkan_core.h>

/*
    each "type implementation" is in the respective .cpp file
    ie: createBuffer in Buffer.cpp, createTexture in Texture.cpp ...
*/

void ResourceManager::cleanup()
{
    // I dont like this way of "clearing" the pools...
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
}