#include "Mesh.hpp"
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <TinyOBJ/tiny_obj_loader.h>
#include <iostream>

VertexInputDescription VertexInputDescription::getDefault()
{
    VertexInputDescription description;

    constexpr VkVertexInputBindingDescription positionBinding = {
        .binding = 0,
        .stride = sizeof(glm::vec3),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    constexpr VkVertexInputBindingDescription attributeBinding = {
        .binding = 1,
        .stride = sizeof(Mesh::VertexAttributes),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    description.bindings = {positionBinding, attributeBinding};

    // Position will be stored at location 0
    constexpr VkVertexInputAttributeDescription positionAttribute = {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0,
    };

    // Normal will be stored at location 1
    constexpr VkVertexInputAttributeDescription normalAttribute = {
        .location = 1,
        .binding = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Mesh::VertexAttributes, normal),
    };

    // Color will be stored at location 2
    VkVertexInputAttributeDescription colorAttribute = {
        .location = 2,
        .binding = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Mesh::VertexAttributes, color),
    };

    // UVs will be stored at location 3
    VkVertexInputAttributeDescription uvAttribute = {
        .location = 3,
        .binding = 1,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Mesh::VertexAttributes, uv),
    };

    description.attributes = {positionAttribute, normalAttribute, colorAttribute, uvAttribute};

    return description;
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

    std::vector<glm::vec3> vertexPositions;
    std::vector<Mesh::VertexAttributes> vertexAttributes;

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

                vertexPositions.emplace_back(vx, vy, vz);
                vertexAttributes.emplace_back(Mesh::VertexAttributes{
                    .normal = {nx, ny, nz},
                    .color = {nx, ny, nz},
                    .uv = {uvx, 1.0 - uvy},
                });
            }
            indexOffset += fv;
        }
    }

    return createMesh(vertexPositions, vertexAttributes, {}, meshName);
}

Handle<Mesh> ResourceManager::createMesh(
    Span<glm::vec3> vertexPositions,
    Span<Mesh::VertexAttributes> vertexAttributes,
    Span<uint32_t> indices,
    std::string_view name)
{
    assert(vertexAttributes.size() == vertexPositions.size());

    // todo: handle naming collisions
    auto iterator = nameToMeshLUT.find(name);
    assert(iterator == nameToMeshLUT.end());

    std::vector<uint32_t> trivialIndices{};
    if(indices.empty())
    {
        trivialIndices.resize(vertexAttributes.size());
        // dont want to pull <algorithm> for iota here
        for(int i = 0; i < trivialIndices.size(); i++)
        {
            trivialIndices[i] = i;
        }
        indices = trivialIndices;
    }
    assert(indices.size() == vertexAttributes.size());

    Handle<Buffer> positionBufferHandle = createBuffer(
        {
            .info =
                {
                    .size = vertexPositions.size() * sizeof(glm::vec3),
                    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .memoryAllocationInfo =
                        {
                            .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        },
                },
            .initialData = vertexPositions.data(),
        },
        std::string{name} + "_positionsBuffer");

    Handle<Buffer> attributesBufferHandle = createBuffer(
        {
            .info =
                {
                    .size = vertexAttributes.size() * sizeof(vertexAttributes[0]),
                    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .memoryAllocationInfo =
                        {
                            .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        },
                },
            .initialData = vertexAttributes.data(),
        },
        std::string{name} + "_attributesBuffer");

    Handle<Buffer> indexBufferHandle = createBuffer(
        {
            .info =
                {
                    .size = indices.size() * sizeof(indices[0]),
                    .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .memoryAllocationInfo =
                        {
                            .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        },
                },
            .initialData = indices.data(),
        },
        std::string{name} + "_indexBuffer");

    Handle<Mesh> newMeshHandle = meshPool.insert(Mesh{
        .name{name},
        .indexCount = uint32_t(indices.size()),
        .indexBuffer = indexBufferHandle,
        .positionBuffer = positionBufferHandle,
        .attributeBuffer = attributesBufferHandle});

    nameToMeshLUT.insert({std::string{name}, newMeshHandle});

    return newMeshHandle;
}

void ResourceManager::deleteMesh(Handle<Mesh> handle)
{
    /*
        todo:
        Enqueue into a per frame queue
        also need to ensure renderer deletes !all! objects from per frame queue during shutdown, ignoring the
            current frame!
    */
    deleteBuffer(get(handle)->positionBuffer);
    deleteBuffer(get(handle)->attributeBuffer);
    meshPool.remove(handle);
}