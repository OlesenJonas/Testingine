#ifndef COMMONTYPES_HLSL
#define COMMONTYPES_HLSL

struct RenderPassData
{
    float4x4 view;
    float4x4 proj;
    float4x4 projView;
    float3 cameraPositionWS;
};

#endif