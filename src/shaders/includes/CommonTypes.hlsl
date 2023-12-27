#ifndef COMMONTYPES_HLSL
#define COMMONTYPES_HLSL

struct CameraMatrices
{
    float4x4 view;
    float4x4 invView;
    float4x4 proj;
    float4x4 invProj;
    float4x4 projView;

    float3 positionWS;
    float pad;
    float4 pad1;
    float4 pad2;
    float4 pad3;
};
struct RenderPassData
{
    CameraMatrices mainCam;
    CameraMatrices drawCam; // can be debug, or main
};

#endif