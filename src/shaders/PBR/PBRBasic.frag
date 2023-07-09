/*
    Based on the learnOpenGL.com glTF spec and glTF-Sample-Viewer
        https://learnopengl.com/PBR/Theory
        https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation
        https://github.com/KhronosGroup/glTF-Sample-Viewer
*/

#include "../Bindless/Setup.hlsl"
#include "PBR.hlsl"

StructForBindless(MaterialInstanceParameters,
    //TODO: check again, but im pretty sure these are all float4 textures
    Handle< Texture2D<float4> > normalTexture;
    Handle< SamplerState > normalSampler;

    Handle< Texture2D<float4> > baseColorTexture;
    Handle< SamplerState > baseColorSampler;

    Handle< Texture2D<float4> > metalRoughTexture;
    Handle< SamplerState > metalRoughSampler;

    Handle< Texture2D<float4> > occlusionTexture;
    Handle< SamplerState > occlusionSampler;
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
    Handle< Placeholder > materialParamsBuffer;
    Handle< ConstantBuffer<MaterialInstanceParameters> > materialInstanceParams;
);

struct VSOutput
{
    [[vk::location(0)]]	float3 vPositionWS : POSITIONT;
    [[vk::location(1)]] float3 vNormalWS : NORMAL0;
    [[vk::location(2)]] float4 vTangentWS : TANGENT0;
    [[vk::location(3)]] float3 vColor : COLOR0;  
    [[vk::location(4)]] float2 vTexCoord : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
    //Have to retrieve the buffer in its own line, otherwise dxc crashes :/
    MaterialInstanceParameters params = shaderInputs.materialInstanceParams.Load();
    Texture2D normalTexture =  params.normalTexture.get();
    SamplerState normalSampler = params.normalSampler.get();

    float3 nrmSampleTS = normalTexture.Sample(normalSampler, input.vTexCoord).xyz;
    nrmSampleTS = 2 * nrmSampleTS - 1;
    // normal map is OpenGL style y direction
    //  todo: make parameter? Can be controlled through texture view?
    nrmSampleTS.y *= -1;

    //TODO: check resulting normals, make screenshot of GLSL result and compare!
    //      think HLSL's cross is left handed, may need to flip order?
    float3 vBitangentWS = input.vTangentWS.w * cross(input.vNormalWS, input.vTangentWS.xyz);
    float3 normalWS = normalize(
        nrmSampleTS.x * input.vTangentWS.xyz +
        nrmSampleTS.y * vBitangentWS   +
        nrmSampleTS.z * input.vNormalWS
    );

    RenderPassData renderPassData = shaderInputs.renderPassData.Load();
    const float3 cameraPositionWS = renderPassData.cameraPositionWS;

    Texture2D colorTexture = params.baseColorTexture.get();
    SamplerState colorSampler = params.baseColorSampler.get();
    float3 baseColor = colorTexture.Sample(colorSampler, input.vTexCoord).rgb;

    Texture2D mrTexture = params.metalRoughTexture.get();
    SamplerState mrSampler = params.metalRoughSampler.get();
    float3 metalRough = mrTexture.Sample(mrSampler, input.vTexCoord).rgb;
    float metal = metalRough.z;
    float roughness = metalRough.y;

    Texture2D occlusionTexture = params.occlusionTexture.get();
    SamplerState occlusionSampler = params.occlusionSampler.get();
    float occlusion = occlusionTexture.Sample(occlusionSampler, input.vTexCoord).x;

    // float3 color = 0.5+0.5*vNormalWS;
    // float3 color = 0.5+0.5*normalWS;
    // float3 color = baseColor;
    // float3 color = rough.xxx;

    float3 Lout = 0.0;

    //todo: move into functions
    const float3 V = normalize(cameraPositionWS - input.vPositionWS);
    float3 F0 = 0.04;
    F0 = lerp(F0, baseColor, metal);
    
    //todo: factor out into shadePointLightPBR(...) 
    //for each point light
    {
        const float3 lightPosWS = float3(2,2,2);
        const float3 lightColor = float3(23.47, 21.31, 20.79);
        
        const float3 Lunnorm = lightPosWS - input.vPositionWS;
        const float3 L = normalize(Lunnorm);
        const float3 H = normalize(L + V);

        const float dist = length(Lunnorm);
        const float attenuation = 1.0 / (dist * dist);
        const float3 radiance = lightColor * attenuation;

        const float3 F = fresnelSchlick(max(dot(H,V),0.0), F0);
        const float NDF = NDFTrowbridgeReitzGGX(normalWS, H, roughness);
        const float G = GeometrySmith(normalWS, V, L, roughness);

        const float3 num = NDF * G * F;
        const float denom = 4.0 * max(dot(normalWS,V),0.0) * max(dot(normalWS,L),0.0) + 0.0001;
        const float3 specular = num/denom;

        const float3 kS = F;
        float3 kD = 1.0 - kS;
        kD *= 1.0 - metal;

        const float NdotL = max(dot(normalWS, L), 0.0);
        Lout += (kD * baseColor / PI + specular) * radiance * NdotL;
    }


    const float3 ambient = 0.03 * baseColor * occlusion;
    Lout += ambient;

    float3 color = Lout;

    return float4(color, 1.0);
}