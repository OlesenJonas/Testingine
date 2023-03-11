#include <basetsd.h>
#include <vulkan/vulkan_core.h>

#include "Mesh.hpp"

#include <TinyOBJ/tiny_obj_loader.h>

#include <iostream>

VertexInputDescription Vertex::getVertexDescription()
{
    VertexInputDescription description;

    VkVertexInputBindingDescription mainBinding = {};
    mainBinding.binding = 0;
    mainBinding.stride = sizeof(Vertex);
    mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    description.bindings.push_back(mainBinding);

    // Position will be stored at location 0
    VkVertexInputAttributeDescription positionAttribute = {};
    positionAttribute.binding = 0;
    positionAttribute.location = 0;
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset = offsetof(Vertex, position);

    // Normal will be stored at location 1
    VkVertexInputAttributeDescription normalAttribute = {};
    normalAttribute.binding = 0;
    normalAttribute.location = 1;
    normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset = offsetof(Vertex, normal);

    // Color will be stored at location 2
    VkVertexInputAttributeDescription colorAttribute = {};
    colorAttribute.binding = 0;
    colorAttribute.location = 2;
    colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    colorAttribute.offset = offsetof(Vertex, color);

    // UVs will be stored at location 3
    VkVertexInputAttributeDescription uvAttribute = {};
    uvAttribute.binding = 0;
    uvAttribute.location = 3;
    uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    uvAttribute.offset = offsetof(Vertex, uv);

    description.attributes.push_back(positionAttribute);
    description.attributes.push_back(normalAttribute);
    description.attributes.push_back(colorAttribute);
    description.attributes.push_back(uvAttribute);

    return description;
}

bool Mesh::loadFromObj(const char* filename)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);
    if(!warn.empty())
    {
        std::cout << "TinyOBJ WARN: " << warn << std::endl;
    }

    if(!err.empty())
    {
        std::cout << "TinyOBJ ERR: " << err << std::endl;
        return false;
    }

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

                Vertex newVert{
                    .position = {vx, vy, vz},
                    .normal = {nx, ny, nz},
                    .color = {nx, ny, nz},
                    // todo: parameterize UV-y flip
                    .uv = {uvx, 1.0 - uvy},
                };

                vertices.push_back(newVert);
            }
            indexOffset += fv;
        }
    }

    return true;
}