#include "../includes/Bindless/Setup.hlsl"
#include "../includes/GPUScene/Access.hlsl"
#include "../includes/VertexAttributes.hlsl"
#include "../includes/CommonTypes.hlsl"

/*
    Not sure if semantic names are needed when compiling only to spirv
*/
struct VSOutput
{
    float4 posOut : SV_POSITION;
    [[vk::location(0)]] float2 vTexCoord : TEXCOORD0;
    [[vk::location(1)]] int instanceIndex : INSTANCE_INDEX;
};

VSOutput main(VSInput input)
{
    const InstanceInfo instanceInfo = getInstanceInfo(input.baseInstance);
    const MeshData meshData = getMeshData(instanceInfo);

    VSOutput vsOut = (VSOutput)0;
    vsOut.instanceIndex = input.baseInstance;

    const StructuredBuffer<uint> indexBuffer = meshData.indexBuffer.get();
    const StructuredBuffer<float3> vertexPositions = meshData.positionBuffer.get();
    const ByteAddressBuffer vertexAttributes = meshData.attributesBuffer.get();
    
    uint vertexIndex = indexBuffer[input.vertexID];

    // ConstantBuffer<RenderPassData> renderPassData = shaderInputs.renderPassData.get();
    // ConstantBuffer<RenderPassData> renderPassData = g_ConstantBuffer_RenderPassData[shaderInputs.renderPassData.resourceHandle];
    ConstantBuffer<RenderPassData> renderPassData = getRenderPassData();
    const float4x4 projViewMatrix = renderPassData.projView;
    // const float4x4 projViewMatrix = g_ConstantBuffer_RenderPassData[shaderInputs.renderPassData.resourceHandle].projView;
    //todo: test mul-ing here already, like in GLSL version
    // const mat4 transformMatrix = getBuffer(RenderPassData, bindlessIndices.renderPassDataBuffer).projView * modelMatrix;
    
    const float3 vertPos = vertexPositions[vertexIndex];
    float4 worldPos = mul(instanceInfo.transform, float4(vertPos,1.0));
    vsOut.posOut = mul(projViewMatrix, worldPos);    
    vsOut.vTexCoord = vertexAttributes.Load<float2>(vertexIndex * meshData.attribStride() + meshData.uvOffset(0));

    return vsOut;
}