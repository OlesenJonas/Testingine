#include "ResourceManager.hpp"
#include "Graphics/Buffer/Buffer.hpp"
#include <TinyOBJ/tiny_obj_loader.h>
#include <intern/Engine/Engine.hpp>
#include <vulkan/vulkan_core.h>

// todo: move each "type implementation" in the types .cpp file?
//          ie: createBuffer into Buffer.cpp, createTexture into Texture.cpp ...

Handle<Buffer> ResourceManager::createBuffer(Buffer::CreateInfo crInfo)
{
    Handle<Buffer> newBufferHandle = bufferPool.insert(Buffer{.info = crInfo.info});

    // if we can guarantee that no two threads access the pools at the same time we can
    // assume that the underlying pointer wont change until this function returns
    Buffer* buffer = bufferPool.get(newBufferHandle);

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

    VmaAllocator& allocator = Engine::get()->getRenderer()->allocator;
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
            Engine::get()->getRenderer()->allocator,
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

        Engine::get()->getRenderer()->immediateSubmit(
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

    return newBufferHandle;
}

Handle<Mesh> ResourceManager::createMesh(const char* file, std::string_view name)
{
    std::string_view fileView{file};
    auto lastDirSep = fileView.find_last_of("/\\");
    auto extensionStart = fileView.find_last_of('.');
    std::string_view meshName =
        name.empty() ? fileView.substr(lastDirSep + 1, extensionStart - lastDirSep - 1) : name;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file, nullptr);
    if(!warn.empty())
    {
        std::cout << "TinyOBJ WARN: " << warn << std::endl;
    }

    if(!err.empty())
    {
        std::cout << "TinyOBJ ERR: " << err << std::endl;
        return {};
    }

    std::vector<Vertex> vertices;

    // TODO: **VERY** unoptimized, barebones
    for(size_t s = 0; s < shapes.size(); s++)
    {
        size_t indexOffset = 0;
        for(size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            int fv = 3;

            for(size_t v = 0; v < fv; v++)
            {
                tinyobj::index_t idx = shapes[s].mesh.indices[indexOffset + v];

                // position
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

                // normal
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

                // uv
                tinyobj::real_t uvx = attrib.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t uvy = attrib.texcoords[2 * idx.texcoord_index + 1];

                vertices.push_back({
                    .position = {vx, vy, vz},
                    .normal = {nx, ny, nz},
                    .color = {nx, ny, nz},
                    // todo: parameterize UV-y flip
                    .uv = {uvx, 1.0 - uvy},
                });
            }
            indexOffset += fv;
        }
    }

    Handle<Buffer> vertexBufferHandle = createBuffer({
        .info =
            {
                .size = vertices.size() * sizeof(Vertex),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .memoryAllocationInfo =
                    {
                        .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    },
            },
        .initialData = vertices.data(),
    });

    // todo: handle naming collisions
    auto iterator = nameToMeshLUT.find(name);
    assert(iterator == nameToMeshLUT.end());

    Handle<Mesh> newMeshHandle = meshPool.insert(
        Mesh{.name{meshName}, .vertexCount = uint32_t(vertices.size()), .vertexBuffer = vertexBufferHandle});

    nameToMeshLUT.insert({std::string{name}, newMeshHandle});

    return newMeshHandle;
}

Handle<Mesh> ResourceManager::createMesh(Span<Vertex> vertices, std::string_view name)
{
    // todo: handle naming collisions
    auto iterator = nameToMeshLUT.find(name);
    assert(iterator == nameToMeshLUT.end());

    Handle<Buffer> vertexBufferHandle = createBuffer({
        .info =
            {
                .size = vertices.size() * sizeof(Vertex),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .memoryAllocationInfo =
                    {
                        .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    },
            },
        .initialData = vertices.data(),
    });

    Handle<Mesh> newMeshHandle = meshPool.insert(
        Mesh{.name{name}, .vertexCount = uint32_t(vertices.size()), .vertexBuffer = vertexBufferHandle});

    nameToMeshLUT.insert({std::string{name}, newMeshHandle});

    return newMeshHandle;
}

Handle<Material> ResourceManager::createMaterial(Material mat, std::string_view name)
{
    // todo: handle naming collisions
    auto iterator = nameToMaterialLUT.find(name);
    assert(iterator == nameToMaterialLUT.end());

    Handle<Material> newMaterialHandle = materialPool.insert(mat);

    nameToMaterialLUT.insert({std::string{name}, newMaterialHandle});

    return newMaterialHandle;
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
    const VmaAllocator* allocator = &Engine::get()->getRenderer()->allocator;
    const VkBuffer vkBuffer = buffer->buffer;
    const VmaAllocation vmaAllocation = buffer->allocation;
    Engine::get()->getRenderer()->deleteQueue.pushBack([=]()
                                                       { vmaDestroyBuffer(*allocator, vkBuffer, vmaAllocation); });
    bufferPool.remove(handle);
}

void ResourceManager::deleteMesh(Handle<Mesh> handle)
{
    /*
        todo:
        Enqueue into a per frame queue
        also need to ensure renderer deletes !all! objects from per frame queue during shutdown, ignoring the
            current frame!
    */
    // currently doesnt own any data, so only need to remove the vertex buffers it points to
    deleteBuffer(get(handle)->vertexBuffer);
    meshPool.remove(handle);
}

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