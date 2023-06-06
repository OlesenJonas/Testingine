#version 450

#extension GL_GOOGLE_include_directive : require

/*
    WIP !
*/

#include "Bindless.glsl"

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 vNormalLS;
layout(location = 3) in vec4 vTangentLS;

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

    vec3 vBitangentLS = vTangentLS.w * cross(vNormalLS, vTangentLS.xyz);
    vec3 normalLS = normalize(
        nrmSampleTS.x * vTangentLS.xyz +
        nrmSampleTS.y * vBitangentLS   +
        nrmSampleTS.z * vNormalLS
    );

    vec3 color = 0.5+0.5*normalLS;

    fragColor = vec4(color, 1.0);
}