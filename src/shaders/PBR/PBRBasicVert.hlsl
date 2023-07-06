/*
    WIP !
*/

#include "../Bindless/Setup.hlsl"
#include "../VertexAttributes.hlsl"
#include "../CommonTypes.hlsl"

/*
    Not sure if semantic names are needed when only compiling to spirv
*/
struct VSOutput
{
    float4 posOut : SV_POSITION;
    [[vk::location(0)]]	float3 vPositionWS : POSITIONT;
    [[vk::location(1)]] float3 vNormalWS : NORMAL0;
    [[vk::location(2)]] float4 vTangentWS : TANGENT0;
    [[vk::location(3)]] float3 vColor : COLOR0;  
    [[vk::location(4)]] float2 vTexCoord : TEXCOORD0;
};

//--------------------

// struct ArrayBuffer
// {
    // RWByteAddressBuffer buffer;
    //alternatively als just store the index here
    // uint resourceIndex;
    // // But if the first one works use that, not sure about codegen /
    // optimizations when copying the index on .get()
// };
//

#define DECLARE_TEMPLATED_TYPE(TYPE, TEMPLATE, DESCR, BINDING)  \
[[vk::binding(BINDING, DESCR)]]                                 \
TYPE<TEMPLATE> g_##TYPE##_##TEMPLATE[];                         \
template <>                                                     \
TYPE<TEMPLATE> Handle< TYPE<TEMPLATE> >::get()                  \
{                                                               \
    return g_##TYPE##_##TEMPLATE[resourceHandle];               \
}                                                               \

DECLARE_TEMPLATED_TYPE(StructuredBuffer, float4x4, STORAGE_BUFFER_SET, 0)
DECLARE_TEMPLATED_TYPE(StructuredBuffer, RenderPassData, STORAGE_BUFFER_SET, 0) 

 
//--------------------

DefineShaderInputs(
    // Frame globals
    uint frameDataBuffer;
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< StructuredBuffer<RenderPassData> > renderPassDataBuffer;
    // Buffer with object transforms and index into that buffer
    Handle< StructuredBuffer<float4x4> > transformBuffer;
    uint transformIndex;
    // Buffer with material/-instance parameters
    uint materialParamsBuffer;
    uint materialInstanceParamsBuffer;
);

VSOutput main(VSInput input)
{
    VSOutput vsOut = (VSOutput)0;

    const float4x4 modelMatrix = shaderInputs.transformBuffer.get()[shaderInputs.transformIndex];
    const float4x4 projViewMatrix = shaderInputs.renderPassDataBuffer.get()[0].projView;
    //todo: test mul-ing here already, like in GLSL version
    // const mat4 transformMatrix = getBuffer(RenderPassData, bindlessIndices.renderPassDataBuffer).projView * modelMatrix;
    
    const float4 worldPos = mul(modelMatrix, float4(input.vPosition,1.0));
    vsOut.vPositionWS = worldPos.xyz;
    vsOut.posOut = mul(projViewMatrix, worldPos);

    vsOut.vColor = input.vColor;
    vsOut.vTexCoord = input.vTexCoord;

    //dont think normalize is needed
    const float3x3 modelMatrix3 = (float3x3)(modelMatrix);
    // just using model matrix directly here. When using non-uniform scaling
    // (which I dont want to rule out) transpose(inverse(model)) is required
    //      GLSL: vNormalWS = normalize( transpose(inverse(modelMatrix3)) * vNormal);
    // but HLSL doesnt have a inverse() function.
    //  TODO: upload transpose(inverse()) as part of transform buffer(? packing?) to GPU
    vsOut.vNormalWS = mul(modelMatrix3, input.vNormal);
    vsOut.vTangentWS = float4(normalize(mul(modelMatrix3, input.vTangent.xyz)),input.vTangent.w);

    return vsOut;
}