#ifndef GPUSCENE_STRUCTS_HLSL
#define GPUSCENE_STRUCTS_HLSL

#include "../ToCPP.hpp"

#ifndef __cplusplus
    #include "../VertexAttributes.hlsl"
    #include "../Bindless/Common.hlsl"
#endif

struct MeshletDescriptor
{
    uint vertexBegin; // offset into vertexIndices
    uint vertexCount; // number of vertices used
    uint primBegin;   // offset into primitiveIndices
    uint primCount;   // number of primitives (triangles) used
    float4 boundingSphere; //position in local coords + radius
};

struct MeshData
{
    // uint indexCount;
    uint additionalUVCount;
    uint meshletCount;
    // ResrcHandle< StructuredBuffer<uint> > indexBuffer;
    ResrcHandle< StructuredBuffer<float3> > positionBuffer;
    ResrcHandle< ByteAddressBuffer > attributesBuffer;
    ResrcHandle< StructuredBuffer<uint> > meshletUniqueVertexIndices;
    //in reality this is a uint8[], indices into meshletUniqueVertexIndices array
    // ResrcHandle< ByteAddressBuffer > meshletPrimitiveIndices;
    ResrcHandle< StructuredBuffer<uint> > meshletPrimitiveIndices;

    ResrcHandle< StructuredBuffer<MeshletDescriptor> > meshletDescriptors;

    uint normalSize() { return sizeof(float3); }
    uint normalOffset() { return 0; }
    uint colorSize() { return sizeof(float3); }
    uint colorOffset() { return normalSize(); }
    uint uvSize() { return sizeof(float2); }
    uint uvOffset(uint i)
    {
        return colorOffset() + colorSize() + i * uvSize();
    }
    uint attribStride()
    {
        return normalSize() + colorSize() + (1 + additionalUVCount) * uvSize();
    }
};

struct InstanceInfo
{
    float4x4 transform;
    float4x4 invTranspTransform;
    float4 boundingSphere; //in world space, center + radius
    uint meshDataIndex;
    uint materialIndex;
    ResrcHandle< Placeholder > materialParamsBuffer;
    ResrcHandle< Placeholder > materialInstanceParamsBuffer;
};

#endif