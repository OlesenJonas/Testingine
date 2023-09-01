#include "../Bindless/Setup.hlsl"

StructForBindless(MaterialInstanceParameters,
    Handle< TextureCube<float4> > cubeMap;
);

DefineShaderInputs(
    // Frame globals
    Handle< Placeholder > frameDataBuffer;
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with object transforms and index into that buffer
    Handle< StructuredBuffer<float4x4> > transformBuffer;
    // Buffer with material/-instance parameters
    Handle< Placeholder > materialParamsBuffer;
    Handle< ConstantBuffer<MaterialInstanceParameters> > materialInstanceParams;
);

struct VSOutput
{
    [[vk::location(0)]]	float3 localPos : POSITIONT;
};

float4 main(VSOutput input) : SV_TARGET
{
    const MaterialInstanceParameters params = shaderInputs.materialInstanceParams.Load();
    const TextureCube<float4> cubeMap = params.cubeMap.get();

    float4 color = cubeMap.Sample(LinearRepeatSampler, normalize(input.localPos));

    return color;
}