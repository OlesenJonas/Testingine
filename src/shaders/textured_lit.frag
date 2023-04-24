#version 450

#extension GL_GOOGLE_include_directive : require

#include "Bindless.glsl"

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

layout(std430, set=UBO_SET, binding = 0) uniform MaterialParametersBuffer {
    uint colorTexture;
} globalMaterialParametersBuffers[];

layout (set = SAMPLED_IMG_SET, binding = 0) uniform sampler globalSamplers[GLOBAL_SAMPLER_COUNT];
layout (set = SAMPLED_IMG_SET, binding = GLOBAL_SAMPLER_COUNT) uniform texture2D globalTexture2Ds[];

layout (push_constant) uniform constants
{
    BindlessIndices bindlessIndices;
};

void main()
{
    const uint colorTextureIndex = globalMaterialParametersBuffers[bindlessIndices.materialParamsBuffer].colorTexture;
    vec3 color = texture(sampler2D(globalTexture2Ds[colorTextureIndex],globalSamplers[0]), texCoord).xyz;
    fragColor = vec4(color, 1.0);
}