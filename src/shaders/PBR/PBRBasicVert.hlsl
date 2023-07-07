#include "../Bindless/Setup.hlsl"
#include "../VertexAttributes.hlsl"
#include "../CommonTypes.hlsl"

/*
    Not sure if semantic names are needed when compiling only to spirv
*/
struct VSOutput
{
    float4 posOut : SV_POSITION;
    [[vk::location(0)]]	float3 vPositionWS : POSITIONT;
    [[vk::location(1)]] float3 vNormalWS : NORMAL0;
    [[vk::location(2)]] float4 vTangentWS : TANGENT0;
    [[vk::location(3)]] float3 vColor : COLOR0;  
    [[vk::location(4)]] float2 vTexCoord : TEXCOORD0;
};

DefineShaderInputs(
    // Frame globals
    Handle< Placeholder > frameDataBuffer;
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with object transforms and index into that buffer
    Handle< StructuredBuffer<float4x4> > transformBuffer;
    uint transformIndex;
    // Buffer with material/-instance parameters
    // using placeholder, since parameter types arent defined here
    Handle< Placeholder > materialParamsBuffer;
    Handle< Placeholder > materialInstanceParams;
);

VSOutput main(VSInput input)
{
    VSOutput vsOut = (VSOutput)0;

    const float4x4 modelMatrix = shaderInputs.transformBuffer.get()[shaderInputs.transformIndex];
    const float4x4 projViewMatrix = shaderInputs.renderPassData.get().projView;
    //todo: test mul-ing here already, like in GLSL version
    // const mat4 transformMatrix = getBuffer(RenderPassData, bindlessIndices.renderPassDataBuffer).projView * modelMatrix;
    
    const float4 worldPos = mul(modelMatrix, float4(input.vPosition,1.0));
    vsOut.vPositionWS = worldPos.xyz;
    vsOut.posOut = mul(projViewMatrix, worldPos);

    vsOut.vColor = input.vColor;
    vsOut.vTexCoord = input.vTexCoord;

    //dont think normalize is needed
    const float3x3 modelMatrix3 = (float3x3)(modelMatrix);
    // just using model matrix directly here. When using non-uniform scaling
    // (which I dont want to rule out) transpose(inverse(model)) is required
    //      GLSL: vNormalWS = normalize( transpose(inverse(modelMatrix3)) * vNormal);
    // but HLSL doesnt have a inverse() function.
    //  TODO: upload transpose(inverse()) as part of transform buffer(? packing?) to GPU
    vsOut.vNormalWS = mul(modelMatrix3, input.vNormal);
    vsOut.vTangentWS = float4(normalize(mul(modelMatrix3, input.vTangent.xyz)),input.vTangent.w);

    return vsOut;
}