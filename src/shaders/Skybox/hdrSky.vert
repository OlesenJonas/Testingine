#include "../includes/Bindless/Setup.hlsl"
#include "../includes/GPUScene/Setup.hlsl"

struct VSOutput
{
    float4 posOut : SV_POSITION;
    [[vk::location(0)]]	float3 localPos : POSITIONT;
    [[vk::location(1)]] int instanceIndex : INSTANCE_INDEX;
};

VSOutput main(VSInput input)
{
    const InstanceInfo instanceInfo = getInstanceInfo(pushConstants.indexInInstanceBuffer);
    const MeshData meshData = getMeshData(instanceInfo);

    const StructuredBuffer<uint> indexBuffer = meshData.meshletPrimitiveIndices.get(); //TODO: wrong to just use meshlet prims here
    const StructuredBuffer<float3> vertexPositions = meshData.positionBuffer.get();
    const ByteAddressBuffer vertexAttributes = meshData.attributesBuffer.get();
    
    uint vertexIndex = indexBuffer[input.vertexID];

    VSOutput vsOut = (VSOutput)0;
    vsOut.instanceIndex = input.baseInstance;

    const float3 vertPos = vertexPositions[vertexIndex];
    //todo: dont just scale up by some large number, instead make forcing depth to 1.0 work!
    float4 worldPos = float4(500*vertPos,1.0);

    vsOut.localPos = vertPos;

    const ConstantBuffer<RenderPassData> renderPassData = getRenderPassData();

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