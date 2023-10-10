#include "../Bindless/Setup.hlsl"
#include "../CommonTypes.hlsl"

/*
    Not sure if semantic names are needed when compiling only to spirv
*/
struct VSOutput
{
    float4 posOut : SV_POSITION;
};

DefineShaderInputs(
    Handle< Texture2D<float4> > inputTex;
);

VSOutput main(uint vertexIndex : SV_VertexID)
{
    VSOutput vsOut = (VSOutput)0;
    vsOut.posOut = float4(
    (vertexIndex / 2u) * 4 - 1.0,
    (vertexIndex & 1u) * 4 - 1.0,
    0.0,
    1.0);
    return vsOut;
}