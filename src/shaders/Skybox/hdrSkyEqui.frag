#include "../Bindless/Setup.hlsl"

StructForBindless(MaterialInstanceParameters,
    Handle< Texture2D<float4> > equirectangularMap;
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

static const float2 invAtan = float2(0.1591, 0.3183);
float2 sampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), -asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

struct VSOutput
{
    [[vk::location(0)]]	float3 localPos : POSITIONT;
};

float4 main(VSOutput input) : SV_TARGET
{
    const MaterialInstanceParameters params = shaderInputs.materialInstanceParams.Load();
    const Texture2D<float4> equirectangularMap = params.equirectangularMap.get();

    float4 color = equirectangularMap.Sample(LinearRepeatSampler, sampleSphericalMap(normalize(input.localPos)));
    return color;
}