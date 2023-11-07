/*
    Based on the learnOpenGL.com glTF spec and glTF-Sample-Viewer
        https://learnopengl.com/PBR/Theory
        https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation
        https://github.com/KhronosGroup/glTF-Sample-Viewer
*/

#include "../includes/Bindless/Setup.hlsl"
#include "../includes/GPUScene/Setup.hlsl"
#include "../includes/MaterialParams.hlsl"
#include "PBR.hlsl"

// http://www.thetenthplanet.de/archives/1180
/*
    N and P need to be in the same space, the resulting matrix
    then transforms tangent space into that space
*/
float3x3 cotangentFrame( float3 N, float3 p, float2 uv )
{
    // get edge vectors of the pixel triangle
    float3 dp1 = ddx_fine( p );
    float3 dp2 = ddy_fine( p );
    float2 duv1 = ddx_fine( uv );
    float2 duv2 = ddy_fine( uv );
 
    // solve the linear system
    float3 dp2perp = cross( dp2, N );
    float3 dp1perp = cross( N, dp1 );
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = rsqrt( max( dot(T,T), dot(B,B) ) );
    return transpose(float3x3( T * invmax, B * invmax, N ));
    //TODO: without transpose, initialize in correct order directly
}

// https://jcgt.org/published/0009/03/04/paper.pdf
//  Surface Gradient–Based Bump Mapping Framework

//todo: no static, wrap into some struct
static float3 sigmaX;
static float3 sigmaY;
static float3 nrmBaseNormal;
static float3 dPdx;
static float3 dPdy;
static float flip_sign;


// Input: vM is tangent space normal in [-1;1].
// Output: convert vM to a derivative.
float2 TspaceNormalToDerivative(float3 vM)
{
    const float scale = 1.0/128.0;
    // Ensure vM delivers a positive third component using abs() and
    // constrain vM.z so the range of the derivative is [-128; 128].
    const float3 vMa = abs(vM);
    const float z_ma = max(vMa.z, scale*max(vMa.x, vMa.y));
    // Set to match positive vertical texture coordinate axis.
    const bool gFlipVertDeriv = false;
    const float s = gFlipVertDeriv ? -1.0 : 1.0;
    return -float2(vM.x, s*vM.y)/z_ma;
}


void GenBasisTB(out float3 vT, out float3 vB, float2 texST)
{
    float2 dSTdx = ddx_fine(texST);
    float2 dSTdy = ddy_fine(texST);
    float det = dot(dSTdx, float2(dSTdy.y, -dSTdy.x));
    float sign_det = det < 0.0 ? -1.0 : 1.0;
    // invC0 represents (dXds, dYds), but we don’t divide
    // by the determinant. Instead, we scale by the sign.
    float2 invC0 = sign_det*float2(dSTdy.y, -dSTdx.y);
    vT = sigmaX*invC0.x + sigmaY*invC0.y;
    if (abs(det) > 0.0) vT = normalize(vT);
    vB = (sign_det*flip_sign)*cross(nrmBaseNormal, vT);
}

float3 SurfgradFromTBN(float2 deriv, float3 vT, float3 vB)
{
    return deriv.x*vT + deriv.y*vB;
}


float3 ResolveNormalFromSurfaceGradient(float3 surfGrad)
{
    return normalize(nrmBaseNormal - surfGrad);
}



MaterialParameters(
    /* TODO: should be part of scene information, not material ? */
    Handle< TextureCube<float4> > irradianceTex;
    Handle< TextureCube<float4> > prefilterTex;

    Handle< Texture2D<float2> > brdfLUT;
);

MaterialInstanceParameters(
    //TODO: check again, but im pretty sure these are all float4 textures
    Handle< Texture2D<float4> > normalTexture;

    Handle< Texture2D<float4> > baseColorTexture;

    Handle< Texture2D<float4> > metalRoughTexture;

    Handle< Texture2D<float4> > occlusionTexture;

    float metallicFactor;
    float roughnessFactor;
);

struct VSOutput
{
    [[vk::location(0)]]	float3 vPositionWS : POSITIONT;
    [[vk::location(1)]] float3 vNormalWS : NORMAL0;
    [[vk::location(2)]] float4 vTangentWS : TANGENT0;
    [[vk::location(3)]] float3 vColor : COLOR0;
    [[vk::location(4)]] float2 vTexCoord : TEXCOORD0;
    [[vk::location(5)]] int baseInstance : BASE_INSTANCE;
};

float4 main(VSOutput input) : SV_TARGET
{
    const InstanceInfo instanceInfo = getInstanceInfo(input.baseInstance);
    const MeshData meshData = getMeshDataBuffer()[instanceInfo.meshDataIndex];

    ConstantBuffer<MaterialParameters> params = getMaterialParameters(instanceInfo);
    ConstantBuffer<MaterialInstanceParameters> instanceParams = getMaterialInstanceParameters(instanceInfo);

    ConstantBuffer<RenderPassData> renderPassData = getRenderPassData();
    const float3 cameraPositionWS = renderPassData.cameraPositionWS;

    float3 normalWS;
    if(instanceParams.normalTexture.isValid())
    {
        Texture2D normalTexture =  instanceParams.normalTexture.get();

        float3 nrmSampleTS = normalTexture.Sample(LinearRepeatSampler, input.vTexCoord).xyz;
        nrmSampleTS = (255.0/127.0) * nrmSampleTS - (128.0/127.0);
        nrmSampleTS = lerp(float3(0,0,1),nrmSampleTS,1.0);
        // normal map is OpenGL style y direction
        //  todo: make parameter? Can be controlled through texture view?
        nrmSampleTS.y *= -1;

        // ---

        // float3 vBitangentWS = input.vTangentWS.w * cross(input.vNormalWS, input.vTangentWS.xyz);
        // normalWS = normalize(
        //     nrmSampleTS.x * input.vTangentWS.xyz +
        //     nrmSampleTS.y * vBitangentWS   +
        //     nrmSampleTS.z * input.vNormalWS
        // );

        // ---

        // nrmSampleTS.x *= -1;
        // float3x3 TBN = cotangentFrame(normalize(input.vNormalWS),cameraPositionWS-input.vPositionWS, input.vTexCoord);
        // normalWS = mul(TBN, nrmSampleTS);

        // ---

        //TODO: even better, use camera relatie world space for better accuracy!
        float3 relSurfPos = input.vPositionWS;
        nrmBaseNormal = normalize(input.vNormalWS);
        dPdx = ddx_fine(relSurfPos);
        dPdy = ddy_fine(relSurfPos);
        sigmaX = dPdx - dot(dPdx, nrmBaseNormal)*nrmBaseNormal;
        sigmaY = dPdy - dot(dPdy, nrmBaseNormal)*nrmBaseNormal;
        flip_sign = dot(dPdy, cross(nrmBaseNormal, dPdx)) < 0 ? -1 : 1;

        float2 deriv = TspaceNormalToDerivative(nrmSampleTS);
        float3 vT;
        float3 vB;
        GenBasisTB(vT, vB, input.vTexCoord);
        float3 surfGrad = SurfgradFromTBN(deriv, vT, vB);
        normalWS = ResolveNormalFromSurfaceGradient(surfGrad);
    }
    else
    {
        normalWS = normalize(input.vNormalWS);
    }

    normalWS = lerp(normalize(input.vNormalWS), normalWS, 1.0);

    Texture2D colorTexture = instanceParams.baseColorTexture.get();
    float3 baseColor = colorTexture.Sample(LinearRepeatSampler, input.vTexCoord).rgb;

    float metal = instanceParams.metallicFactor;
    float roughness = instanceParams.roughnessFactor;
    if(instanceParams.metalRoughTexture.isValid())
    {
        Texture2D mrTexture = instanceParams.metalRoughTexture.get();
        float3 metalRough = mrTexture.Sample(LinearRepeatSampler, input.vTexCoord).rgb;
        metal *= metalRough.z;
        roughness *= metalRough.y;
    }

    float occlusion = 1.0;
    if(instanceParams.occlusionTexture.isValid())
    {   
        Texture2D occlusionTexture = instanceParams.occlusionTexture.get();
        occlusion = occlusionTexture.Sample(LinearRepeatSampler, input.vTexCoord).x;
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

    Lout += ambient;

    // ---------------------

    float3 color = Lout;
    
    //TODO: remove
    // color = clamp(color,0.0,1.0)*0.0001;
    // color += normalWS*0.5+0.5;

    return float4(color, 1.0);
}