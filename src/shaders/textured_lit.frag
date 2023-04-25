#version 450

#extension GL_GOOGLE_include_directive : require

#include "Bindless.glsl"

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

MaterialParameters(
    TextureHandle colorTexture;
);

layout (push_constant) uniform constants
{
    BindlessIndices bindlessIndices;
};

void main()
{
    const TextureHandle colorTexture = getMaterialParams(bindlessIndices.materialParamsBuffer).colorTexture;
    vec3 color = texture(sampler2D(Texture2D(colorTexture),Samplers[0]), texCoord).xyz;
    fragColor = vec4(color, 1.0);
}