#version 450

#extension GL_GOOGLE_include_directive : require

#include "../Bindless.glsl"

layout (location = 0) in vec3 localPos;

layout(location = 0) out vec4 fragColor;

MaterialInstanceParameters(
    TextureHandle equirectangularMap;
    SamplerHandle defaultSampler;
);

layout (push_constant, std430) uniform constants
{
    BindlessIndices bindlessIndices;
};

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 sampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), -asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    const TextureHandle equirectangularMap = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).equirectangularMap;
    const SamplerHandle defaultSampler = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).defaultSampler;

    vec3 color = texture(sampler2D(Texture2D(equirectangularMap),Samplers[defaultSampler.index]), 
        sampleSphericalMap(normalize(localPos))
    ).xyz;

    fragColor = vec4(color, 1.0);
}