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
    float4x4 transform;
};

#endif