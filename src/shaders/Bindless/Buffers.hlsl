#ifndef BINDLESS_BUFFERS_HLSL
#define BINDLESS_BUFFERS_HLSL

#include "Common.hlsl"

// ---------------------

// ConstantBuffers cant correctly be stored inside variables
// (https://github.com/microsoft/DirectXShaderCompiler/issues/5401)
// So Handle< ConstantBuffer<> > only offers a method to load the
// contained data directly from the bindless array
template<typename X>
struct Handle< ConstantBuffer<X> >
{
    uint resourceHandle;

    X Load();

    ConstantBuffer<X> get();
};

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

#define ENABLE_STRUCTURED_ACCESS(TYPE)                                      \
DECLARE_TEMPLATED_ARRAY(StructuredBuffer, TYPE, STORAGE_BUFFER_SET, 0)      \
template <>                                                                 \
StructuredBuffer<TYPE> Handle< StructuredBuffer<TYPE> >::get()              \
{                                                                           \
    return g_StructuredBuffer_##TYPE[resourceHandle];                       \
}

#define ENABLE_CONSTANT_ACCESS(TYPE)                                        \
DECLARE_TEMPLATED_ARRAY(ConstantBuffer, TYPE, UNIFORM_BUFFER_SET, 0)        \
template <>                                                                 \
TYPE Handle< ConstantBuffer<TYPE> >::Load()                                 \
{                                                                           \
    return g_ConstantBuffer_##TYPE[resourceHandle];                         \
}                                                                           \
template <>                                                                 \
ConstantBuffer<TYPE> Handle< ConstantBuffer<TYPE> >::get()                  \
{                                                                           \
    return g_ConstantBuffer_##TYPE[resourceHandle];                         \
}                                                                           

// ---------------------

#define StructForBindless(name, members)                            \
struct name                                                         \
{                                                                   \
    members                                                         \
};                                                                  \
ENABLE_STRUCTURED_ACCESS(name)                                      \
ENABLE_CONSTANT_ACCESS(name)

// --------------------- Some predefined buffers

ENABLE_STRUCTURED_ACCESS(uint)
ENABLE_STRUCTURED_ACCESS(float3)
ENABLE_STRUCTURED_ACCESS(float4)
ENABLE_STRUCTURED_ACCESS(float4x4)

#include "../CommonTypes.hlsl"
ENABLE_CONSTANT_ACCESS(RenderPassData)

#include "../VertexAttributes.hlsl"
ENABLE_STRUCTURED_ACCESS(VertexAttributes)

#include "../GPUSceneStructs.hlsl"
ENABLE_STRUCTURED_ACCESS(RenderItem);
ENABLE_STRUCTURED_ACCESS(InstanceInfo);
// currently 0th buffer is hardcoded (c++ side) to be the render item buffer
// static Handle< StructuredBuffer<RenderItem> > globalRenderItemBuffer = Handle< StructuredBuffer<RenderItem> >(0);
#define RENDER_ITEM_BUFFER g_StructuredBuffer_RenderItem[0]

// ---------------------

// RWStructuredBuffer<float4x4> float4x4RWBuffers[];
// Spirv codegen currently doesnt allow RWStructuredBuffer arrays which is a big problem for this bindless setup
//  open PR: https://github.com/microsoft/DirectXShaderCompiler/pull/4663
//  replace the following hack once this is accepted
// "Temporary Solution":
//      Have custom templated RWStructuredBuffer type that just wraps a byteadressbuffer and wraps bab.load() / .store()
/*
    Implementation: TODO:
*/
// struct ArrayBuffer
// {
    // RWByteAddressBuffer buffer;
    //alternatively als just store the index here
    // uint resourceIndex;
    // // But if the first one works use that, not sure about codegen /
    // optimizations when copying the index on .get()
// };
//

#endif