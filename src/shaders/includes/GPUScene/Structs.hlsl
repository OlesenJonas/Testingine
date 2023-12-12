#ifndef GPUSCENE_STRUCTS_HLSL
#define GPUSCENE_STRUCTS_HLSL

#include "../VertexAttributes.hlsl"
#include "../Bindless/Common.hlsl"

struct MeshletDescriptor
{
    uint vertexBegin; // offset into vertexIndices
    uint vertexCount; // number of vertices used
    uint primBegin;   // offset into primitiveIndices
    uint primCount;   // number of primitives (triangles) used
};

struct MeshData
{
    // uint indexCount;
    uint additionalUVCount;
    uint meshletCount;
    // Handle< StructuredBuffer<uint> > indexBuffer;
    Handle< StructuredBuffer<float3> > positionBuffer;
    Handle< ByteAddressBuffer > attributesBuffer;
    Handle< StructuredBuffer<uint> > meshletVertexIndices;
    // Handle< ByteAddressBuffer > meshletPrimitiveIndices;
    Handle< StructuredBuffer<uint> > meshletPrimitiveIndices;

    Handle< StructuredBuffer<MeshletDescriptor> > meshletDescriptors;

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
    uint meshDataIndex;
    uint materialIndex;
    Handle< Placeholder > materialParamsBuffer;
    Handle< Placeholder > materialInstanceParamsBuffer;
};

#endif