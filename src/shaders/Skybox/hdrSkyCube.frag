#version 450

#extension GL_GOOGLE_include_directive : require

#include "../Bindless.glsl"

layout (location = 0) in vec3 localPos;

layout(location = 0) out vec4 fragColor;

MaterialInstanceParameters(
    TextureHandle cubeMap;
    SamplerHandle defaultSampler;
);

layout (push_constant, std430) uniform constants
{
    BindlessIndices bindlessIndices;
};

layout (set = SAMPLED_IMG_SET, binding = GLOBAL_SAMPLER_COUNT) uniform textureCube global_textureCube[];

void main()
{
    const TextureHandle cubeMap = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).cubeMap;
    const SamplerHandle defaultSampler = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).defaultSampler;

    vec3 color = texture(
        samplerCube(global_textureCube[cubeMap.index],Samplers[defaultSampler.index]), 
        normalize(localPos)
    ).xyz;

    fragColor = vec4(color, 1.0);
}