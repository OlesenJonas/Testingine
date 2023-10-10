/*
    Based on the learnOpenGL.com glTF spec and glTF-Sample-Viewer
        https://learnopengl.com/PBR/Theory
        https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation
        https://github.com/KhronosGroup/glTF-Sample-Viewer
*/

#include "../Bindless/Setup.hlsl"

StructForBindless(MaterialInstanceParameters,
    Handle< Texture2D<float4> > texture;
);

DefineShaderInputs(
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with information about all instances that are being rendered
    Handle< StructuredBuffer<InstanceInfo> > instanceBuffer;
);

struct VSOutput
{
    [[vk::location(0)]] float2 vTexCoord : TEXCOORD0;
    [[vk::location(1)]] int instanceIndex : INSTANCE_INDEX;
};

float4 main(VSOutput input) : SV_TARGET
{
    const StructuredBuffer<InstanceInfo> instanceInfoBuffer = shaderInputs.instanceBuffer.get();
    const InstanceInfo instanceInfo = instanceInfoBuffer[input.instanceIndex];

    ConstantBuffer<MaterialInstanceParameters> instanceParams = instanceInfo.materialInstanceParamsBuffer.specify<ConstantBuffer<MaterialInstanceParameters> >().get();
    Texture2D tex = instanceParams.texture.get();
    float3 baseColor = tex.Sample(LinearRepeatSampler, input.vTexCoord).rgb;

    return float4(baseColor, 1.0);
}