/*
    Based on the learnOpenGL.com glTF spec and glTF-Sample-Viewer
        https://learnopengl.com/PBR/Theory
        https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation
        https://github.com/KhronosGroup/glTF-Sample-Viewer
*/

#include "../includes/Bindless/Setup.hlsl"
#include "../includes/GPUScene/Setup.hlsl"
#include "../includes/MaterialParams.hlsl"
#include "../includes/NormalMapping.hlsl"
#include "PBR.hlsl"

MaterialParameters(
    /* TODO: should be part of scene information, not material ? */
    Handle< TextureCube<float4> > irradianceTex;
    Handle< TextureCube<float4> > prefilterTex;

    Handle< Texture2D<float2> > brdfLUT;
);

MaterialInstanceParameters(
    //TODO: check again, but im pretty sure these are all float4 textures
    Handle< Texture2D<float4> > normalTexture;
    uint normalUVSet;

    Handle< Texture2D<float4> > baseColorTexture;
    uint baseColorUVSet;

    float4 normalTexOffsetScale;
    float4 baseColorTexOffsetScale;

    Handle< Texture2D<float4> > metalRoughTexture;
    uint metalRoughUVSet;
    float metallicFactor;
    float roughnessFactor;

    float4 metalRoughTexOffsetScale;

    Handle< Texture2D<float4> > occlusionTexture;
    uint occlusionUVSet;
    uint2 pad; //TODO
    float4 occlusionTexOffsetScale;
);

struct VSOutput
{
    [[vk::location(0)]]	float3 vPositionWS : POSITIONT;
    [[vk::location(1)]] float3 vNormalWS : NORMAL0;
    [[vk::location(2)]] float3 vColor : COLOR0;
    [[vk::location(3)]] float2 vTexCoord0 : TEXCOORD0;
    [[vk::location(4)]] float2 vTexCoord1 : TEXCOORD1;
    [[vk::location(5)]] float2 vTexCoord2 : TEXCOORD2;
    [[vk::location(6)]] int baseInstance : BASE_INSTANCE;
};

float4 main(VSOutput input) : SV_TARGET
{
    const InstanceInfo instanceInfo = getInstanceInfo(input.baseInstance);
    const MeshData meshData = getMeshDataBuffer()[instanceInfo.meshDataIndex];

    ConstantBuffer<MaterialParameters> params = getMaterialParameters(instanceInfo);
    ConstantBuffer<MaterialInstanceParameters> instanceParams = getMaterialInstanceParameters(instanceInfo);

    ConstantBuffer<RenderPassData> renderPassData = getRenderPassData();
    const float3 cameraPositionWS = renderPassData.cameraPositionWS;

    //Dont like this, optimally would have different shader variants were correct uvs are selected at compile time
    float2 uvs[3] = {input.vTexCoord0,input.vTexCoord1,input.vTexCoord2};

    float3 normalWS;
    if(instanceParams.normalTexture.isNonNull())
    {
        float2 nrmUVs = uvs[instanceParams.normalUVSet];
        nrmUVs = instanceParams.normalTexOffsetScale.xy + (nrmUVs * instanceParams.normalTexOffsetScale.zw);

        Texture2D normalTexture =  instanceParams.normalTexture.get();

        float3 nrmSampleTS = normalTexture.Sample(LinearRepeatSampler, nrmUVs).xyz;
        nrmSampleTS = (255.0/127.0) * nrmSampleTS - (128.0/127.0);
        nrmSampleTS = lerp(float3(0,0,1),nrmSampleTS,1.0);
        // normal map is OpenGL style y direction
        //  todo: make parameter? Can be controlled through texture view?
        nrmSampleTS.y *= -1;

        // ---

        //TODO: even better, use camera relatie world space for better accuracy!
        SurfGrad::initParams(input.vPositionWS, normalize(input.vNormalWS));

        float3 vT;
        float3 vB;
        SurfGrad::genBasis(nrmUVs, vT, vB);

        float3 surfGrad = SurfGrad::fromTangentNormal(nrmSampleTS, vT, vB);
        normalWS = SurfGrad::resolveToNormal(1*surfGrad);
    }
    else
    {
        normalWS = normalize(input.vNormalWS);
    }

    normalWS = lerp(normalize(input.vNormalWS), normalWS, 1.0);

    
    float2 colorUVs = uvs[instanceParams.baseColorUVSet];
    colorUVs = instanceParams.baseColorTexOffsetScale.xy + (colorUVs * instanceParams.baseColorTexOffsetScale.zw);
    Texture2D colorTexture = instanceParams.baseColorTexture.get();
    float3 baseColor = colorTexture.Sample(LinearRepeatSampler, colorUVs).rgb;

    float metal = instanceParams.metallicFactor;
    float roughness = instanceParams.roughnessFactor;
    if(instanceParams.metalRoughTexture.isNonNull())
    {
        float2 mrUVs = uvs[instanceParams.metalRoughUVSet];
        mrUVs = instanceParams.metalRoughTexOffsetScale.xy + (mrUVs * instanceParams.metalRoughTexOffsetScale.zw);
        Texture2D mrTexture = instanceParams.metalRoughTexture.get();
        float3 metalRough = mrTexture.Sample(LinearRepeatSampler, mrUVs).rgb;
        metal *= metalRough.z;
        roughness *= metalRough.y;
    }

    float occlusion = 1.0;
    if(instanceParams.occlusionTexture.isNonNull())
    {   
        float2 aoUVs = uvs[instanceParams.occlusionUVSet];
        aoUVs = instanceParams.occlusionTexOffsetScale.xy + (aoUVs * instanceParams.occlusionTexOffsetScale.zw);
        Texture2D occlusionTexture = instanceParams.occlusionTexture.get();
        occlusion = occlusionTexture.Sample(LinearRepeatSampler, aoUVs).x;
    }

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

    // Ambient lighting
    TextureCube<float4> irradianceTex = params.irradianceTex.get();
    TextureCube<float4> prefilteredEnvTex = params.prefilterTex.get();
    Texture2D<float2> brdfLUT = params.brdfLUT.get();

    float3 R = reflect(-V,normalWS);

    const float MAX_REFLECTION_LOD = 4.0;
    float3 prefilteredColor = prefilteredEnvTex.SampleLevel(LinearRepeatSampler, R, roughness*MAX_REFLECTION_LOD).rgb;
    float3 F = fresnelSchlickRoughness(max(dot(normalWS, V),0.0),F0,roughness);
    float2 brdfLUTCoords = float2(
        max(dot(normalWS, V), 0.0),
        roughness
    );
    float2 envBRDF = brdfLUT.SampleLevel(LinearClampSampler, brdfLUTCoords, 0.0);
    float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
    
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metal;

    float3 irradiance = irradianceTex.SampleLevel(LinearRepeatSampler, normalWS, 0).rgb;
    float3 diffuse = irradiance * baseColor;

    float3 ambient = (kD * diffuse + specular) * occlusion;

    Lout += 0.5* ambient;

    // ---------------------

    float3 color = Lout;
    // float3 color = 0.0001*clamp(Lout,0,1);
    // color += metal;
    
    return float4(color, 1.0);
}