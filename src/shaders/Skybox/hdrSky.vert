#include "../Bindless/Setup.hlsl"
#include "../VertexAttributes.hlsl"
#include "../CommonTypes.hlsl"

struct VSOutput
{
    float4 posOut : SV_POSITION;
    [[vk::location(0)]]	float3 localPos : POSITIONT;
};

DefineShaderInputs(
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

    vsOut.localPos = input.vPosition;

    // const StructuredBuffer<float4x4> transformBuffer = globalTransformBuffer.get();
    const StructuredBuffer<float4x4> transformBuffer = GLOBAL_TRANSFORMS;
    const float4x4 modelMatrix = transformBuffer[input.baseInstance];

    const RenderPassData renderPassData = shaderInputs.renderPassData.Load();
    const float4x4 projMatrix = renderPassData.proj;
    const float4x4 viewMatrix = renderPassData.view;
    //remove translation component from view matrix
    const float4x4 viewMatrixNoTranslate = float4x4(
        viewMatrix[0].xyz,0.0,
        viewMatrix[1].xyz,0.0,
        viewMatrix[2].xyz,0.0,
        viewMatrix[3].xyz,1.0
    );
    
    //todo: dont just scale up by some large number, just make forcing depth to 1.0 work!
    float4 worldPos = mul(modelMatrix, float4(500*input.vPosition,1.0));

    vsOut.posOut = mul(projMatrix, mul(viewMatrixNoTranslate, worldPos));

    return vsOut;
}