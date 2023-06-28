#version 450

#extension GL_GOOGLE_include_directive : require

/*
    WIP !
*/

/*
    Based on the learnOpenGL.com glTF spec and glTF-Sample-Viewer
        https://learnopengl.com/PBR/Theory
        https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation
        https://github.com/KhronosGroup/glTF-Sample-Viewer
*/

#include "Bindless.glsl"
#include "PBR.glsl"

layout (location = 0) in vec3 vPositionWS;
layout (location = 1) in vec3 outColor;
layout (location = 2) in vec2 texCoord;
layout (location = 3) in vec3 vNormalWS;
layout (location = 4) in vec4 vTangentWS;

layout(location = 0) out vec4 fragColor;

MaterialInstanceParameters(
    TextureHandle normalTexture;
    SamplerHandle normalSampler;

    TextureHandle baseColorTexture;
    SamplerHandle baseColorSampler;

    TextureHandle metalRoughTexture;
    SamplerHandle metalRoughSampler;

    TextureHandle occlusionTexture;
    SamplerHandle occlusionSampler;
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
    //  todo: make parameter
    nrmSampleTS.y *= -1;

    vec3 vBitangentWS = vTangentWS.w * cross(vNormalWS, vTangentWS.xyz);
    vec3 normalWS = normalize(
        nrmSampleTS.x * vTangentWS.xyz +
        nrmSampleTS.y * vBitangentWS   +
        nrmSampleTS.z * vNormalWS
    );

    const vec3 cameraPositionWS = getBuffer(RenderPassData, bindlessIndices.renderPassDataBuffer).cameraPositionWS;

    const TextureHandle colorTexture = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).baseColorTexture;
    const SamplerHandle colorSampler = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).baseColorSampler;
    vec3 baseColor = texture(sampler2D(Texture2D(colorTexture),Samplers[colorSampler.index]), texCoord).xyz;

    const TextureHandle mrTexture = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).metalRoughTexture;
    const SamplerHandle mrSampler = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).metalRoughSampler;
    vec3 metalRough = texture(sampler2D(Texture2D(mrTexture),Samplers[mrSampler.index]), texCoord).xyz;
    float metal = metalRough.z;
    float roughness = metalRough.y;

    const TextureHandle occlusionTexture = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).occlusionTexture;
    const SamplerHandle occlusionSampler = getMaterialInstanceParams(bindlessIndices.materialInstanceParamsBuffer).occlusionSampler;
    float occlusion = texture(sampler2D(Texture2D(occlusionTexture),Samplers[occlusionSampler.index]), texCoord).x;

    // vec3 color = 0.5+0.5*vNormalWS;
    // vec3 color = 0.5+0.5*normalWS;
    // vec3 color = baseColor;
    // vec3 color = rough.xxx;

    vec3 Lout = vec3(0.0);

    //todo: move into functions
    const vec3 V = normalize(cameraPositionWS - vPositionWS);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor, metal);
    
    //todo: factor out into shadePointLightPBR(...) 
    //for each point light
    {
        const vec3 lightPosWS = vec3(2,2,2);
        const vec3 lightColor = vec3(23.47, 21.31, 20.79);
        
        const vec3 Lunnorm = lightPosWS - vPositionWS;
        const vec3 L = normalize(Lunnorm);
        const vec3 H = normalize(L + V);

        const float dist = length(Lunnorm);
        const float attenuation = 1.0 / (dist * dist);
        const vec3 radiance = lightColor * attenuation;

        const vec3 F = fresnelSchlick(max(dot(H,V),0.0), F0);
        const float NDF = NDFTrowbridgeReitzGGX(normalWS, H, roughness);
        const float G = GeometrySmith(normalWS, V, L, roughness);

        const vec3 num = NDF * G * F;
        const float denom = 4.0 * max(dot(normalWS,V),0.0) * max(dot(normalWS,L),0.0) + 0.0001;
        const vec3 specular = num/denom;

        const vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metal;

        const float NdotL = max(dot(normalWS, L), 0.0);
        Lout += (kD * baseColor / PI + specular) * radiance * NdotL;
    }


    const vec3 ambient = vec3(0.03) * baseColor * occlusion;
    Lout += ambient;

    vec3 color = Lout;
    fragColor = vec4(color, 1.0);
}