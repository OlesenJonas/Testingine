#include "../Bindless/Setup.hlsl"
#include "../VertexAttributes.hlsl"
#include "../CommonTypes.hlsl"

/*
    Not sure if semantic names are needed when compiling only to spirv
*/
struct VSOutput
{
    float4 posOut : SV_POSITION;
    [[vk::location(0)]]	float3 vPositionWS : POSITIONT;
    [[vk::location(1)]] float3 vNormalWS : NORMAL0;
    [[vk::location(2)]] float4 vTangentWS : TANGENT0;
    [[vk::location(3)]] float3 vColor : COLOR0;  
    [[vk::location(4)]] float2 vTexCoord : TEXCOORD0;
    [[vk::location(5)]] int baseInstance : BASE_INSTANCE;
};

DefineShaderInputs(
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with information about all instances that are being rendered
    Handle< StructuredBuffer<InstanceInfo> > instanceBuffer;

    //TODO: remove
    Handle< StructuredBuffer<uint> > indexBuffer;
    Handle< StructuredBuffer<float3> > positionBuffer;
    Handle< StructuredBuffer<VertexAttributes> > attributesBuffer;
);

VSOutput main(VSInput input)
{
    const StructuredBuffer<InstanceInfo> instanceInfoBuffer = shaderInputs.instanceBuffer.get();
    const InstanceInfo instanceInfo = instanceInfoBuffer[input.baseInstance];
    
    const StructuredBuffer<RenderItem> renderItemBuffer = RENDER_ITEM_BUFFER;
    const RenderItem renderItem = renderItemBuffer[instanceInfo.renderItemIndex];

    // const StructuredBuffer<uint> indexBuffer = shaderInputs.indexBuffer.get();
    // const StructuredBuffer<float3> vertexPositions = shaderInputs.positionBuffer.get();
    const StructuredBuffer<uint> indexBuffer = renderItem.indexBuffer.get();
    const StructuredBuffer<float3> vertexPositions = renderItem.positionBuffer.get();
    const StructuredBuffer<VertexAttributes> vertexAttributes = renderItem.attributesBuffer.get();

    // ConstantBuffer<RenderPassData> renderPassData = shaderInputs.renderPassData.get();
    // ConstantBuffer<RenderPassData> renderPassData = g_ConstantBuffer_RenderPassData[shaderInputs.renderPassData.resourceHandle];
    RenderPassData renderPassData = shaderInputs.renderPassData.Load();
    const float4x4 projViewMatrix = renderPassData.projView;
    // const float4x4 projViewMatrix = g_ConstantBuffer_RenderPassData[shaderInputs.renderPassData.resourceHandle].projView;
    //todo: test mul-ing here already, like in GLSL version
    // const mat4 transformMatrix = getBuffer(RenderPassData, bindlessIndices.renderPassDataBuffer).projView * modelMatrix;

    VSOutput vsOut = (VSOutput)0;
    vsOut.baseInstance = input.baseInstance;

    uint vertexIndex = indexBuffer[input.vertexID];
    const float3 vertPos = vertexPositions[vertexIndex];
    float4 worldPos = mul(instanceInfo.transform, float4(vertPos,1.0));

    vsOut.vPositionWS = worldPos.xyz;
    vsOut.posOut = mul(projViewMatrix, worldPos);

    vsOut.vColor = vertexAttributes[vertexIndex].color;
    vsOut.vTexCoord = vertexAttributes[vertexIndex].uv;

    //dont think normalize is needed
    const float3x3 modelMatrix3 = (float3x3)(instanceInfo.transform);
    // just using model matrix directly here. When using non-uniform scaling
    // (which I dont want to rule out) transpose(inverse(model)) is required
    //      GLSL: vNormalWS = normalize( transpose(inverse(modelMatrix3)) * vNormal);
    // but HLSL doesnt have a inverse() function.
    //  TODO: upload transpose(inverse()) as part of transform buffer(? packing?) to GPU
    vsOut.vNormalWS = mul(modelMatrix3, vertexAttributes[vertexIndex].normal);
    float4 vertexTangent = vertexAttributes[vertexIndex].tangent;
    vsOut.vTangentWS = float4(normalize(mul(modelMatrix3, vertexTangent.xyz)),vertexTangent.w);

    return vsOut;
}