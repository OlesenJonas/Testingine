#ifndef GPUSCENE_ACCESS_HLSL
#define GPUSCENE_ACCESS_HLSL

#include "Structs.hlsl"
#include "BindlessDeclarations.hlsl"

// currently 0th buffer is hardcoded (c++ side) to be the render item buffer
StructuredBuffer<MeshData> getMeshDataBuffer()
{
    return g_StructuredBuffer_MeshData[0];
}

MeshData getMeshData(InstanceInfo instanceInfo)
{
    return getMeshDataBuffer()[instanceInfo.meshDataIndex];
}

#ifndef NO_DEFAULT_PUSH_CONSTANTS

#include "DefaultPushConstants.hlsl"
StructuredBuffer<InstanceInfo> getInstanceBuffer()
{
    return pushConstants.instanceBuffer.get();
}

InstanceInfo getInstanceInfo(int instanceIndex)
{
    return getInstanceBuffer()[instanceIndex];
}

ConstantBuffer<RenderPassData> getRenderPassData()
{
    return pushConstants.renderPassData.get();
}
#endif

#endif