#define NO_DEFAULT_PUSH_CONSTANTS
#include "../includes/Bindless/Setup.hlsl"

DefinePushConstants(
    ResrcHandle< Texture2D<float4> > sourceTex;
    ResrcHandle< RWTexture2DArray<float4> > targetTex;
);

static const float2 invAtan = float2(0.1591, 0.3183);
float2 sampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), -asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

[numthreads(8, 8, 1)]
void main(
    uint3 GlobalInvocationID : SV_DispatchThreadID,
    uint3 LocalInvocationID : SV_GroupThreadID
)
{
    int3 id = GlobalInvocationID;

    int3 size;
    RWTexture2DArray<float4> targetTex = pushConstants.targetTex.get();
    targetTex.GetDimensions(size.x, size.y, size.z);
    float2 uv = float2(id.xy + 0.5) / size.xy;
    uv.y = 1.0 - uv.y;
    uv = 2.0 * uv - 1.0;

    float3 localPos;
    if(id.z == 0)
        localPos = float3(1.0, uv.y, -uv.x); // pos x
    else if(id.z == 1)
        localPos = float3(-1.0, uv.y, uv.x); // neg x
    else if(id.z == 2)
        localPos = float3(uv.x, 1.0, -uv.y); // pos y
    else if(id.z == 3)
        localPos = float3(uv.x, -1.0, uv.y); // neg y
    else if(id.z == 4)
        localPos = float3(uv.x, uv.y, 1.0); // pos z
    else if(id.z == 5)
        localPos = float3(-uv.x, uv.y, -1.0); // neg z

    float3 color =
        pushConstants.sourceTex.get().SampleLevel(
            LinearRepeatSampler,
            sampleSphericalMap(normalize(localPos)),
            0
        ).xyz;

    targetTex[id] = float4(color, 1.0);
}