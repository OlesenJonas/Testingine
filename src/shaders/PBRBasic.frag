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
    const TextureHandle normalTexture = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).normalTexture;
    const SamplerHandle normalSampler = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).normalSampler;
    vec3 color = texture(sampler2D(Texture2D(normalTexture),Samplers[normalSampler.index]), texCoord).xyz;
    // vec3 color = texture(sampler2D(global_texture2D[2],Samplers[normalSampler.index]), texCoord).xyz;
    fragColor = vec4(color, 1.0);
}