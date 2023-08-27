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