#include "../includes/Bindless/Setup.hlsl"
#include "../includes/GPUScene/Setup.hlsl"
#include "../includes/MaterialParams.hlsl"

MaterialInstanceParameters(
    Handle< TextureCube<float4> > cubeMap;
);

struct VSOutput
{
    [[vk::location(0)]]	float3 localPos : POSITIONT;
    [[vk::location(1)]] int instanceIndex : INSTANCE_INDEX;
};

float4 main(VSOutput input) : SV_TARGET
{
    const InstanceInfo instanceInfo = getInstanceInfo(input.instanceIndex);

    const ConstantBuffer<MaterialInstanceParameters> params = getMaterialInstanceParameters(instanceInfo);
    const TextureCube<float4> cubeMap = params.cubeMap.get();

    float4 color = cubeMap.Sample(LinearRepeatSampler, normalize(input.localPos));

    return color;
}