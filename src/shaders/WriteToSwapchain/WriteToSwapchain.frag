#define NO_DEFAULT_PUSH_CONSTANTS
#include "../includes/Bindless/Setup.hlsl"

DefinePushConstants(
    Handle< Texture2D<float4> > inputTex;
);

struct VSOutput
{
    float4 fragCoord : SV_POSITION;
};

float4 main(VSOutput input) : SV_TARGET
{
    Texture2D tex = pushConstants.inputTex.get();

    float3 color = tex.Load(int3(input.fragCoord.xy, 0.0)).rgb;

    return float4(color, 1.0);
}