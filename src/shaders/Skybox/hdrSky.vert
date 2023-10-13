#include "../Bindless/Setup.hlsl"
#include "../VertexAttributes.hlsl"
#include "../CommonTypes.hlsl"

struct VSOutput
{
    float4 posOut : SV_POSITION;
    [[vk::location(0)]]	float3 localPos : POSITIONT;
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

    const StructuredBuffer<uint> indexBuffer = renderItem.indexBuffer.get();
    const StructuredBuffer<float3> vertexPositions = renderItem.positionBuffer.get();
    const StructuredBuffer<VertexAttributes> vertexAttributes = renderItem.attributesBuffer.get();
    
    uint vertexIndex = indexBuffer[input.vertexID];

    VSOutput vsOut = (VSOutput)0;
    vsOut.instanceIndex = input.baseInstance;

    const float3 vertPos = vertexPositions[vertexIndex];
    //todo: dont just scale up by some large number, instead make forcing depth to 1.0 work!
    float4 worldPos = float4(500*vertPos,1.0);

    vsOut.localPos = vertPos;

    const ConstantBuffer<RenderPassData> renderPassData = shaderInputs.renderPassData.get();

    const float4x4 projMatrix = renderPassData.proj;
    const float4x4 viewMatrix = renderPassData.view;
    //remove translation component from view matrix
    const float4x4 viewMatrixNoTranslate = float4x4(
        viewMatrix[0].xyz,0.0,
        viewMatrix[1].xyz,0.0,
        viewMatrix[2].xyz,0.0,
        viewMatrix[3].xyz,1.0
    );

    vsOut.posOut = mul(projMatrix, mul(viewMatrixNoTranslate, worldPos));

    return vsOut;
}