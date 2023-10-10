#include "../Bindless/Setup.hlsl"
#include "../VertexAttributes.hlsl"
#include "../CommonTypes.hlsl"

/*
    Not sure if semantic names are needed when compiling only to spirv
*/
struct VSOutput
{
    float4 posOut : SV_POSITION;
    [[vk::location(0)]] float2 vTexCoord : TEXCOORD0;
    [[vk::location(1)]] int instanceIndex : INSTANCE_INDEX;
};

DefineShaderInputs(
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with information about all instances that are being rendered
    Handle< StructuredBuffer<InstanceInfo> > instanceBuffer;
);

VSOutput main(VSInput input)
{
    const StructuredBuffer<InstanceInfo> instanceInfoBuffer = shaderInputs.instanceBuffer.get();
    const InstanceInfo instanceInfo = instanceInfoBuffer[input.baseInstance];
    
    const StructuredBuffer<RenderItem> renderItemBuffer = RENDER_ITEM_BUFFER;
    const RenderItem renderItem = renderItemBuffer[instanceInfo.renderItemIndex];

    VSOutput vsOut = (VSOutput)0;
    vsOut.instanceIndex = input.baseInstance;

    const StructuredBuffer<uint> indexBuffer = renderItem.indexBuffer.get();
    const StructuredBuffer<float3> vertexPositions = renderItem.positionBuffer.get();
    const StructuredBuffer<VertexAttributes> vertexAttributes = renderItem.attributesBuffer.get();
    
    uint vertexIndex = indexBuffer[input.vertexID];

    // ConstantBuffer<RenderPassData> renderPassData = shaderInputs.renderPassData.get();
    // ConstantBuffer<RenderPassData> renderPassData = g_ConstantBuffer_RenderPassData[shaderInputs.renderPassData.resourceHandle];
    RenderPassData renderPassData = shaderInputs.renderPassData.Load();
    const float4x4 projViewMatrix = renderPassData.projView;
    // const float4x4 projViewMatrix = g_ConstantBuffer_RenderPassData[shaderInputs.renderPassData.resourceHandle].projView;
    //todo: test mul-ing here already, like in GLSL version
    // const mat4 transformMatrix = getBuffer(RenderPassData, bindlessIndices.renderPassDataBuffer).projView * modelMatrix;
    
    const float3 vertPos = vertexPositions[vertexIndex];
    float4 worldPos = mul(instanceInfo.transform, float4(vertPos,1.0));
    vsOut.posOut = mul(projViewMatrix, worldPos);    
    vsOut.vTexCoord = vertexAttributes[vertexIndex].uv;

    return vsOut;
}