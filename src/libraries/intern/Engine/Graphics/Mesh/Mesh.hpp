#pragma once

#include "../Buffer/Buffer.hpp"

#include <Datastructures/Pool/Handle.hpp>
#include <Datastructures/Span.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <mikktspace/mikktspace.h>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

// todo: just contain two/three Buffer::Handle for index + position/(position+attributes) !!

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

    static constexpr auto MAX_SUBMESHES = 6;

    struct RenderData
    {
        uint32_t indexCount = 0;
        Buffer::Handle indexBuffer = Buffer::Handle::Invalid();
        Buffer::Handle positionBuffer = Buffer::Handle::Invalid();
        Buffer::Handle attributeBuffer = Buffer::Handle::Invalid();

        uint32_t gpuIndex = 0xFFFFFFFF;
    };
    using SubMeshes = std::array<RenderData, MAX_SUBMESHES>;
    static_assert(std::is_trivially_move_constructible<SubMeshes>::value);
    static_assert(std::is_trivially_destructible<SubMeshes>::value);

    /*
        uint is GPU buffer index, TODO: wrap in type?
        Also store submesh count? so dont need to interate over all? (atm MAX is 6 so not that bad)
    */
    using Handle = Handle<std::string, SubMeshes>;

    // --------------------------------------------------------

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