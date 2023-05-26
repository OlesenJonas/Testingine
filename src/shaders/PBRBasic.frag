#version 450

#extension GL_GOOGLE_include_directive : require

/*
    WIP !
*/

#include "Bindless.glsl"

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 texCoord;

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
    // const TextureHandle colorTexture = getMaterialInstanceParams(bindlessIndices.materialParamsBuffer).colorTexture;
    // const SamplerHandle blockySampler = getMaterialInstanceParams(bindlessIndices.materialParamsBuffer).blockySampler;
    // vec3 color = texture(sampler2D(Texture2D(colorTexture),Samplers[blockySampler.index]), texCoord).xyz;
    vec3 color = vec3(1.0);
    fragColor = vec4(color, 1.0);
}