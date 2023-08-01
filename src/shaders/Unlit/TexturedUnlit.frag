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
    // Frame globals
    Handle< Placeholder > frameDataBuffer;
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with object transforms and index into that buffer
    Handle< StructuredBuffer<float4x4> > transformBuffer;
    uint transformIndex;
    // Buffer with material/-instance parameters
    Handle< Placeholder > materialParams;
    Handle< ConstantBuffer<MaterialInstanceParameters> > materialInstanceParams;
);

struct VSOutput
{
    [[vk::location(0)]] float2 vTexCoord : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
    // MaterialInstanceParameters instanceParams = shaderInputs.materialInstanceParams.Load();
    // Texture2D tex = instanceParams.texture.get();
    Texture2D tex = shaderInputs.materialInstanceParams.get().texture.get();
    float3 baseColor = tex.Sample(LinearRepeatSampler, input.vTexCoord).rgb;

    return float4(baseColor, 1.0);
}