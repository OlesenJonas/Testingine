#ifndef GPUSCENE_STRUCTS_HLSL
#define GPUSCENE_STRUCTS_HLSL

#include "../VertexAttributes.hlsl"
#include "../Bindless/Common.hlsl"

struct MeshData
{
    uint indexCount;
    uint additionalUVCount;
    Handle< StructuredBuffer<uint> > indexBuffer;
    Handle< StructuredBuffer<float3> > positionBuffer;
    Handle< ByteAddressBuffer > attributesBuffer;

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