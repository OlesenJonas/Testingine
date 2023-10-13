#ifndef BINDLESS_BUFFERS_HLSL
#define BINDLESS_BUFFERS_HLSL

#include "Common.hlsl"

// ---------------------

// ByteAddressBuffers are always treated as storage buffers :(

[[vk::binding(0, STORAGE_BUFFER_SET)]]                                   
ByteAddressBuffer g_ByteAddressBuffer[];

template<>
struct Handle< ByteAddressBuffer >
{
    uint resourceHandle;
    ByteAddressBuffer get()
    {
        return g_ByteAddressBuffer[resourceHandle];
    }
};

// ---------------------

#define ENABLE_STRUCTURED_ACCESS(TYPE)                                              \
DECLARE_RESOURCE_ARRAY_TEMPLATED(StructuredBuffer, TYPE, STORAGE_BUFFER_SET, 0)     \
IMPLEMENT_HANDLE_GETTER(StructuredBuffer, TYPE)

#define ENABLE_RWSTRUCTURED_ACCESS(TYPE)                                            \
DECLARE_RESOURCE_ARRAY_TEMPLATED(RWStructuredBuffer, TYPE, STORAGE_BUFFER_SET, 0)   \
IMPLEMENT_HANDLE_GETTER(RWStructuredBuffer, TYPE)

#define ENABLE_CONSTANT_ACCESS(TYPE)                                                \
DECLARE_RESOURCE_ARRAY_TEMPLATED(ConstantBuffer, TYPE, UNIFORM_BUFFER_SET, 0)       \
IMPLEMENT_HANDLE_GETTER(ConstantBuffer, TYPE)

#define ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(TYPE)   \
ENABLE_STRUCTURED_ACCESS(TYPE)                      \
ENABLE_RWSTRUCTURED_ACCESS(TYPE)                    

#define ENABLE_BINDLESS_BUFFER_ACCESS(TYPE)     \
ENABLE_STRUCTURED_ACCESS(TYPE)                  \
ENABLE_RWSTRUCTURED_ACCESS(TYPE)                \
ENABLE_CONSTANT_ACCESS(TYPE)

// ---------------------

#define StructForBindless(name, members)                            \
struct name                                                         \
{                                                                   \
    members                                                         \
};                                                                  \
ENABLE_BINDLESS_BUFFER_ACCESS(name)                                      

// --------------------- Some predefined buffers

ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(uint)
ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(uint2)
ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(uint3)
ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(uint4)

ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(float)
ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(float2)
ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(float3)
ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(float4)

ENABLE_BINDLESS_STRUC_BUFFER_ACCESS(float4x4)

#include "../CommonTypes.hlsl"
ENABLE_BINDLESS_BUFFER_ACCESS(RenderPassData)

#include "../VertexAttributes.hlsl"
ENABLE_BINDLESS_BUFFER_ACCESS(VertexAttributes)

#include "../GPUSceneStructs.hlsl"
ENABLE_BINDLESS_BUFFER_ACCESS(RenderItem);
ENABLE_BINDLESS_BUFFER_ACCESS(InstanceInfo);

// currently 0th buffer is hardcoded (c++ side) to be the render item buffer
// static Handle< StructuredBuffer<RenderItem> > globalRenderItemBuffer = Handle< StructuredBuffer<RenderItem> >(0);
#define RENDER_ITEM_BUFFER g_StructuredBuffer_RenderItem[0]

#endif