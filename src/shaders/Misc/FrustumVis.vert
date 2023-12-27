#define NO_DEFAULT_PUSH_CONSTANTS
#include "../includes/Bindless/Setup.hlsl"
#include "../includes/GPUScene/Setup.hlsl"

DefinePushConstants(
    ResrcHandle< ConstantBuffer<RenderPassData> > renderPassData;
);

struct VSOutput
{
    float4 posOut : SV_POSITION;
};

VSOutput main(
    uint vertexID : SV_VertexID
)
{
    const ConstantBuffer<RenderPassData> renderPassData = pushConstants.renderPassData.get();

    VSOutput vsOut = (VSOutput)0;

    uint lineIndex = vertexID % 8u;
    float3 vertPos;
    vertPos.x = (lineIndex >> 0) & 1u;
    vertPos.y = (lineIndex >> 1) & 1u;
    vertPos.z = (lineIndex >> 2) & 1u;
    if(vertexID >= 16)
        vertPos = vertPos.zyx;
    else if(vertexID >= 8)
        vertPos = vertPos.yxz;
    vertPos.xy = 2.0*vertPos.xy-1;

    float4 posMainCamSpace = mul(renderPassData.mainCam.invProj, float4(vertPos,1.0));
    posMainCamSpace /= posMainCamSpace.w;
    float4 posWorldSpace = mul(renderPassData.mainCam.invView, posMainCamSpace);
    vsOut.posOut = mul(renderPassData.drawCam.projView, posWorldSpace);
    return vsOut;
}