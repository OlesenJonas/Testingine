#include "Mesh.hpp"
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <TinyOBJ/tiny_obj_loader.h>
#include <glm/gtc/epsilon.hpp>
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

    // Tangents will be stored at location 2
    constexpr VkVertexInputAttributeDescription tangentAttribute = {
        .location = 2,
        .binding = 1,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(Mesh::VertexAttributes, tangent),
    };

    // Color will be stored at location 3
    VkVertexInputAttributeDescription colorAttribute = {
        .location = 3,
        .binding = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Mesh::VertexAttributes, color),
    };

    // UVs will be stored at location 4
    VkVertexInputAttributeDescription uvAttribute = {
        .location = 4,
        .binding = 1,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Mesh::VertexAttributes, uv),
    };

    description.attributes = {positionAttribute, normalAttribute, tangentAttribute, colorAttribute, uvAttribute};

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
    Span<const Mesh::PositionType> vertexPositions,
    Span<const Mesh::VertexAttributes> vertexAttributes,
    Span<const uint32_t> indices,
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

    Handle<Buffer> positionBufferHandle = createBuffer(Buffer::CreateInfo{
        .debugName = (std::string{name} + "_positionsBuffer"),
        .size = vertexPositions.size() * sizeof(glm::vec3),
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::VertexBuffer | ResourceState::TransferDst,
        .initialState = ResourceState::VertexBuffer,
        .initialData = {(uint8_t*)vertexPositions.data(), vertexPositions.size() * sizeof(vertexPositions[0])},
    });

    Handle<Buffer> attributesBufferHandle = createBuffer(Buffer::CreateInfo{
        .debugName = (std::string{name} + "_attributesBuffer"),
        .size = vertexAttributes.size() * sizeof(vertexAttributes[0]),
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::VertexBuffer | ResourceState::TransferDst,
        .initialState = ResourceState::VertexBuffer,
        .initialData = {(uint8_t*)vertexAttributes.data(), vertexAttributes.size() * sizeof(vertexAttributes[0])},
    });

    Handle<Buffer> indexBufferHandle = createBuffer(Buffer::CreateInfo{
        .debugName = (std::string{name} + "_indexBuffer"),
        .size = indices.size() * sizeof(indices[0]),
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::IndexBuffer | ResourceState::TransferDst,
        .initialState = ResourceState::IndexBuffer,
        .initialData = {(uint8_t*)indices.data(), indices.size() * sizeof(indices[0])},
    });

    Handle<Mesh> newMeshHandle = meshPool.insert(Mesh{
        .name{name},
        .indexCount = uint32_t(indices.size()),
        .indexBuffer = indexBufferHandle,
        .positionBuffer = positionBufferHandle,
        .attributeBuffer = attributesBufferHandle});

    nameToMeshLUT.insert({std::string{name}, newMeshHandle});

    return newMeshHandle;
}

void ResourceManager::free(Handle<Mesh> handle)
{
    /*
        todo:
        Enqueue into a per frame queue
        also need to ensure renderer deletes !all! objects from per frame queue during shutdown, ignoring the
            current frame!
    */
    free(get(handle)->positionBuffer);
    free(get(handle)->attributeBuffer);
    meshPool.remove(handle);
}

// for mikktspace

int Mesh::mktGetNumFaces(const SMikkTSpaceContext* pContext)
{
    auto* userData = reinterpret_cast<MikkTSpaceUserData*>(pContext->m_pUserData);
    assert(userData->indices.size() % 3 == 0);
    return static_cast<int>(userData->indices.size() / 3);
}
int Mesh::mktGetNumVerticesOfFace(const SMikkTSpaceContext* pContext, const int iFace)
{
    (void)pContext;
    (void)iFace;
    return 3;
}
void Mesh::mktGetPosition(const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
{
    auto* userData = reinterpret_cast<MikkTSpaceUserData*>(pContext->m_pUserData);
    const auto& indices = userData->indices;
    const int vertIndex = indices[iFace * 3 + iVert];
    const auto& pos = userData->vertexPositions[vertIndex];
    memcpy(fvPosOut, &pos.x, 3 * sizeof(PositionType::value_type));
}
void Mesh::mktGetNormal(const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert)
{
    auto* userData = reinterpret_cast<MikkTSpaceUserData*>(pContext->m_pUserData);
    const auto& indices = userData->indices;
    const int vertIndex = indices[iFace * 3 + iVert];
    const auto& nrm = userData->vertexAttributes[vertIndex].normal;
    memcpy(fvNormOut, &nrm.x, 3 * sizeof(float));
}
void Mesh::mktGetTexCoord(const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert)
{
    auto* userData = reinterpret_cast<MikkTSpaceUserData*>(pContext->m_pUserData);
    const auto& indices = userData->indices;
    const int vertIndex = indices[iFace * 3 + iVert];
    const auto& uv = userData->vertexAttributes[vertIndex].uv;
    memcpy(fvTexcOut, &uv.x, 2 * sizeof(TexCoordType::value_type));
}
void Mesh::mktSetTSpaceBasic(
    const SMikkTSpaceContext* pContext,
    const float fvTangent[3],
    const float fSign,
    const int iFace,
    const int iVert)
{
    auto* userData = reinterpret_cast<MikkTSpaceUserData*>(pContext->m_pUserData);
    const auto& indices = userData->indices;
    const int vertIndex = indices[iFace * 3 + iVert];

    glm::vec4& tangent = userData->vertexAttributes[vertIndex].tangent;

    // until I add splitting and then "re-welding" the mesh, just assert that the mesh
    // has been split correctly before import (assert that no two tangent basis share the same vert)
    const glm::vec4 temp{fvTangent[0], fvTangent[1], fvTangent[2], fSign};
    const glm::vec4 emptyTang{0.f, 0.f, 0.f, 1.f};
    // assert(tangent == glm::vec4(0.f, 0.f, 0.f, 1.0f) || glm::all(glm::epsilonEqual(temp, tangent, 0.01f)));
    assert(tangent == emptyTang || glm::all(glm::epsilonEqual(temp, tangent, 0.01f)));
    assert(fSign == 1.0f || fSign == -1.0f && "Tangent sign does not have abs value of 1.0f");

    tangent = temp;
}

void Mesh::generateTangents(
    Span<PositionType> positions, Span<VertexAttributes> attributes, Span<IndexType> indices)
{
    MikkTSpaceUserData mktUserData = {positions, attributes, indices};
    SMikkTSpaceContext mktContext = {nullptr};
    SMikkTSpaceInterface mktInterface = {nullptr};
    mktContext.m_pUserData = &mktUserData;
    mktContext.m_pInterface = &mktInterface;
    mktInterface.m_getNumFaces = mktGetNumFaces;
    mktInterface.m_getNumVerticesOfFace = mktGetNumVerticesOfFace;
    mktInterface.m_getPosition = mktGetPosition;
    mktInterface.m_getTexCoord = mktGetTexCoord;
    mktInterface.m_getNormal = mktGetNormal;
    mktInterface.m_setTSpaceBasic = mktSetTSpaceBasic;
    auto res = genTangSpaceDefault(&mktContext);
    assert(res);
}