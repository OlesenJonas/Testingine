#include "../Bindless/Setup.hlsl"

StructForBindless(MaterialInstanceParameters,
    Handle< Texture2D<float4> > equirectangularMap;
);

DefineShaderInputs(
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with information about all instances that are being rendered
    Handle< StructuredBuffer<InstanceInfo> > instanceBuffer;
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
    [[vk::location(1)]] int instanceIndex : INSTANCE_INDEX;
};

float4 main(VSOutput input) : SV_TARGET
{
    const StructuredBuffer<InstanceInfo> instanceInfoBuffer = shaderInputs.instanceBuffer.get();
    const InstanceInfo instanceInfo = instanceInfoBuffer[input.instanceIndex];

    const MaterialInstanceParameters params = instanceInfo.materialInstanceParamsBuffer.specify<ConstantBuffer<MaterialInstanceParameters> >().Load();
    const Texture2D<float4> equirectangularMap = params.equirectangularMap.get();

    float4 color = equirectangularMap.Sample(LinearRepeatSampler, sampleSphericalMap(normalize(input.localPos)));
    return color;
}