#ifndef BINDLESS_BUFFERS_HLSL
#define BINDLESS_BUFFERS_HLSL

#include "Common.hlsl"

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

ENABLE_STRUCTURED_ACCESS(float4x4)

#include "../CommonTypes.hlsl"

ENABLE_CONSTANT_ACCESS(RenderPassData)

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