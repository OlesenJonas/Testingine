#ifndef GPUSCENESTRUCTS_HLSL
#define GPUSCENESTRUCTS_HLSL

#include "Bindless/Common.hlsl"
#include "VertexAttributes.hlsl"

struct RenderItem
{
    Handle< StructuredBuffer<uint> > indexBuffer;
    uint indexCount;
    Handle< StructuredBuffer<float3> > positionBuffer;
    Handle< StructuredBuffer<VertexAttributes> > attributesBuffer;
};

struct InstanceInfo
{
    float4x4 transform;
    uint renderItemIndex;
    uint materialIndex;
    Handle< Placeholder > materialParamsBuffer;
    Handle< Placeholder > materialInstanceParamsBuffer;
};

#endif