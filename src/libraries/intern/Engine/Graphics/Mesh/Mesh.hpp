#pragma once

#include "../Buffer/Buffer.hpp"

#include <Datastructures/Pool.hpp>
#include <Datastructures/Span.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <mikktspace/mikktspace.h>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

// todo: just contain two/three Handle<Buffer> for index + position/(position+attributes) !!

struct VertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    static VertexInputDescription getDefault();
};

struct Mesh
{
    // todo: replace leftover hardcoded types
    using IndexType = uint32_t;
    using PositionType = glm::vec3;
    using TexCoordType = glm::vec2;

    struct VertexAttributes
    {
        glm::vec3 normal;
        glm::vec4 tangent{0.f, 0.f, 0.f, 1.0f};
        glm::vec3 color{0.f, 0.f, 0.f};
        TexCoordType uv;
    };

    // todo: does this need to be here?
    //  I feel like the actual GPU sided stuff, ie vertexcount, buffer handles etc should be seperate from the
    //  scene management realted stuff like name, parent, children etc
    std::string name{};

    uint32_t indexCount = 0;
    Handle<Buffer> indexBuffer;
    Handle<Buffer> positionBuffer;
    Handle<Buffer> attributeBuffer;

    // for mikktspace
    struct MikkTSpaceUserData
    {
        Span<PositionType> vertexPositions;
        Span<VertexAttributes> vertexAttributes;
        Span<IndexType> indices;
    };
    static int mktGetNumFaces(const SMikkTSpaceContext* pContext);
    static int mktGetNumVerticesOfFace(const SMikkTSpaceContext* pContext, const int iFace);
    static void
    mktGetPosition(const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert);
    static void
    mktGetNormal(const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert);
    static void
    mktGetTexCoord(const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert);
    static void mktSetTSpaceBasic(
        const SMikkTSpaceContext* pContext,
        const float fvTangent[3],
        const float fSign,
        const int iFace,
        const int iVert);
    static void
    generateTangents(Span<PositionType> positions, Span<VertexAttributes> attributes, Span<IndexType> indices);
};