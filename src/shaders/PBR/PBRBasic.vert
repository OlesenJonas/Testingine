#include "../includes/Bindless/Setup.hlsl"
#include "../includes/GPUScene/Setup.hlsl"
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

VSOutput main(VSInput input)
{
    const InstanceInfo instanceInfo = getInstanceInfo(input.baseInstance);
    const MeshData meshData = getMeshDataBuffer()[instanceInfo.meshDataIndex];

    const StructuredBuffer<uint> indexBuffer = meshData.indexBuffer.get();
    const StructuredBuffer<float3> vertexPositions = meshData.positionBuffer.get();
    const StructuredBuffer<VertexAttributes> vertexAttributes = meshData.attributesBuffer.get();

    // ConstantBuffer<RenderPassData> renderPassData = shaderInputs.renderPassData.get();
    // ConstantBuffer<RenderPassData> renderPassData = g_ConstantBuffer_RenderPassData[shaderInputs.renderPassData.resourceHandle];
    ConstantBuffer<RenderPassData> renderPassData = getRenderPassData();
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

    const float3x3 modelMatrix3 = (float3x3)(instanceInfo.transform);
    const float3x3 invTranspModelMatrix3 = (float3x3)(instanceInfo.invTranspTransform);
    vsOut.vNormalWS = normalize(mul(invTranspModelMatrix3, vertexAttributes[vertexIndex].normal));
    float4 vertexTangent = vertexAttributes[vertexIndex].tangent;
    vsOut.vTangentWS = float4(normalize(mul(modelMatrix3, vertexTangent.xyz)),vertexTangent.w);

    return vsOut;
}