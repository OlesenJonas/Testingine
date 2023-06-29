#ifndef PBR_GLSL
#define PBR_GLSL

#ifndef PI
    #define PI 3.141592653589793238462643383279502884197169399375105820974944
#endif 

float NDFTrowbridgeReitzGGX(vec3 N, vec3 H, float roughness)
{
    const float a = roughness * roughness; 
    const float a2 = a*a;
    const float NdotH = max(dot(N,H), 0.0);
    const float NdotH2 = NdotH * NdotH;

    const float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num/denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    const float r = (roughness + 1.0);
    float k = (r*r)/8.0;

    const float num = NdotV;
    const float denom = NdotV * (1.0 - k) + k;
    
    return num/denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    const float NdotV = max(dot(N,V),0.0);
    const float NdotL = max(dot(N,L),0.0);
    const float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    const float ggx2 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}



#endif