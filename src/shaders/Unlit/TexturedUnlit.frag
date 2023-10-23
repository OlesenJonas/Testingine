#include "../includes/Bindless/Setup.hlsl"
#include "../includes/GPUScene/Setup.hlsl"
#include "../includes/MaterialParams.hlsl"

MaterialInstanceParameters(
    Handle< Texture2D<float4> > texture;
);

struct VSOutput
{
    [[vk::location(0)]] float2 vTexCoord : TEXCOORD0;
    [[vk::location(1)]] int instanceIndex : INSTANCE_INDEX;
};

float4 main(VSOutput input) : SV_TARGET
{
    const InstanceInfo instanceInfo = getInstanceInfo(input.instanceIndex);

    ConstantBuffer<MaterialInstanceParameters> instanceParams = getMaterialInstanceParameters(instanceInfo);
    Texture2D tex = instanceParams.texture.get();
    float3 baseColor = tex.Sample(LinearRepeatSampler, input.vTexCoord).rgb;

    return float4(baseColor, 1.0);
}