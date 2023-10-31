#ifndef GPUSCENE_STRUCTS_HLSL
#define GPUSCENE_STRUCTS_HLSL

#include "../VertexAttributes.hlsl"
#include "../Bindless/Common.hlsl"

struct MeshData
{
    Handle< StructuredBuffer<uint> > indexBuffer;
    uint indexCount;
    Handle< StructuredBuffer<float3> > positionBuffer;
    Handle< StructuredBuffer<VertexAttributes> > attributesBuffer;
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