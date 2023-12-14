#ifndef GPUSCENE_ACCESS_HLSL
#define GPUSCENE_ACCESS_HLSL

#include "Structs.hlsl"
#include "BindlessDeclarations.hlsl"


#if __SHADER_TARGET_STAGE != __SHADER_STAGE_MESH && !defined EditorLinter

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

#elif __SHADER_TARGET_STAGE == __SHADER_STAGE_MESH || defined EditorLinter //Shader stage is mesh
    
    //TODO: move into common file (MeshShaderAccessFix.hlsl)
    // Storing/Passing/Returning buffers in mesh shaders is still broken..... 
    //  https://github.com/microsoft/DirectXShaderCompiler/issues/6087
    // So need ugly defines to access everything "in-line"
    #define GetGlobalBuffer(BufferType, InnerType, BufferHandle) g_##BufferType##_##InnerType[BufferHandle.resourceIndex]
    #define StrucBuffFromHandle(Type, Handle) g_StructuredBuffer_##Type[Handle.resourceIndex]
    #define ByteBuffFromHandle(Handle) g_ByteAddressBuffer[Handle.resourceIndex]


    // currently 0th buffer is hardcoded (c++ side) to be the render item buffer
    #define getMeshDataBuffer() g_StructuredBuffer_MeshData[0]

    MeshData getMeshData(InstanceInfo instanceInfo)
    {
        return getMeshDataBuffer()[instanceInfo.meshDataIndex];
    }

    #ifndef NO_DEFAULT_PUSH_CONSTANTS

    #include "DefaultPushConstants.hlsl"
    #define getInstanceBuffer() g_StructuredBuffer_InstanceInfo[pushConstants.instanceBuffer.resourceIndex]

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

#endif