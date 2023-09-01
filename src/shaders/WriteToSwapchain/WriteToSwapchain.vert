#include "../Bindless/Setup.hlsl"
#include "../VertexAttributes.hlsl"
#include "../CommonTypes.hlsl"

/*
    Not sure if semantic names are needed when compiling only to spirv
*/
struct VSOutput
{
    float4 posOut : SV_POSITION;
    [[vk::location(0)]] float2 vTexCoord : TEXCOORD0;
};

DefineShaderInputs(
    // Frame globals
    Handle< Placeholder > frameDataBuffer;
    // Resolution, matrices (differs in eg. shadow and default pass)
    // Handle< ConstantBuffer_fix<RenderPassData> > renderPassData;
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with material/-instance parameters
    // using placeholder, since parameter types arent defined here
    Handle< Placeholder > materialParamsBuffer;
    Handle< Placeholder > materialInstanceParams;
);

VSOutput main(VSInput input)
{
    VSOutput vsOut = (VSOutput)0;
    vsOut.posOut = float4(input.vPosition,1.0);
    vsOut.vTexCoord = input.vTexCoord;
    return vsOut;
}