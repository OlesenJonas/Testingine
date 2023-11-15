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

/*
struct VertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    static VertexInputDescription getDefault();
};
*/

struct Mesh
{
    // todo: replace leftover hardcoded types
    using IndexType = uint32_t;
    using PositionType = glm::vec3;
    using TexCoordType = glm::vec2;

    /*
        Vertex Attributes have to be layed out as:
            float3 normal
            float3 color (float4 ?)
            float2[] uvs
        Passed as byte array to functions together with this format struct
    */
    struct VertexAttributeFormat
    {
        uint32_t additionalUVCount = 0;
        size_t normalSize() const;
        size_t normalOffset() const;
        size_t colorSize() const;
        size_t colorOffset() const;
        size_t uvSize() const;
        size_t uvOffset(uint32_t i) const;
        size_t combinedSize() const;
    };
    template <uint32_t numExtraUVs>
    struct BasicVertexAttributes
    {
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 uvs[1 + numExtraUVs];
    };

    static constexpr auto MAX_SUBMESHES = 6;

    struct RenderData
    {
        // This does not define GPU representation, change GPUMeshData instead!
        uint32_t indexCount = 0;
        uint32_t additionalUVCount = 0;
        Buffer::Handle indexBuffer = Buffer::Handle::Invalid();
        Buffer::Handle positionBuffer = Buffer::Handle::Invalid();
        Buffer::Handle attributeBuffer = Buffer::Handle::Invalid();

        uint32_t gpuIndex = 0xFFFFFFFF;
    };

    /*
        uint is GPU buffer index, TODO: wrap in type?
        Also store submesh count? so dont need to interate over all? (atm MAX is 6 so not that bad)
    */
    using Handle = Handle<std::string, RenderData>;

    // --------------------------------------------------------

    /*
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
    */
};