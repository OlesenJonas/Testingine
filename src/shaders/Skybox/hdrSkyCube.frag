#include "../Bindless/Setup.hlsl"

StructForBindless(MaterialInstanceParameters,
    Handle< TextureCube<float4> > cubeMap;
);

DefineShaderInputs(
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with information about all instances that are being rendered
    Handle< StructuredBuffer<InstanceInfo> > instanceBuffer;
);

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
    const TextureCube<float4> cubeMap = params.cubeMap.get();

    float4 color = cubeMap.Sample(LinearRepeatSampler, normalize(input.localPos));

    return color;
}