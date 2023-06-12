#version 450

#extension GL_GOOGLE_include_directive : require

/*
    WIP !
*/

#include "Bindless.glsl"

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 vNormalWS;
layout(location = 3) in vec4 vTangentWS;

layout(location = 0) out vec4 fragColor;

MaterialInstanceParameters(
    TextureHandle normalTexture;
    SamplerHandle normalSampler;
);

layout (push_constant) uniform constants
{
    BindlessIndices bindlessIndices;
};

void main()
{
    const TextureHandle normalTexture = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).normalTexture;
    const SamplerHandle normalSampler = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).normalSampler;

    vec3 nrmSampleTS = 2*texture(sampler2D(Texture2D(normalTexture),Samplers[normalSampler.index]), texCoord).xyz - 1;
    // normal map is OpenGL style y direction
    nrmSampleTS.y *= -1;

    vec3 vBitangentWS = vTangentWS.w * cross(vNormalWS, vTangentWS.xyz);
    vec3 normalWS = normalize(
        nrmSampleTS.x * vTangentWS.xyz +
        nrmSampleTS.y * vBitangentWS   +
        nrmSampleTS.z * vNormalWS
    );

    vec3 color = 0.5+0.5*normalWS;
    // vec3 color = 0.5+0.5*vNormalWS;

    fragColor = vec4(color, 1.0);
}