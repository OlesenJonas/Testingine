#ifndef BINDLESS_BUFFERS_HLSL
#define BINDLESS_BUFFERS_HLSL

#include "Common.hlsl"

// ---------------------

/*
    Returning a constant buffer from a function is currently bugged
    (https://github.com/microsoft/DirectXShaderCompiler/issues/5401),
    so we need to return another type. This has a good amount of downsides
    (like not being able to access individual members)
    but the alternative is not having constant buffers at all :shrug:
    Maybe I'll come up with a better solution thats not too far away from the original
    ConstantBuffer syntax, but the current solution is ok for now
*/
template <typename T>
struct ConstantBuffer_fix
{
    uint resourceHandle;

    T Load();
};

// ---------------------

#define ENABLE_STRUCTURED_ACCESS(TYPE)                                      \
DECLARE_TEMPLATED_ARRAY(StructuredBuffer, TYPE, STORAGE_BUFFER_SET, 0)      \
template <>                                                                 \
StructuredBuffer<TYPE> Handle< StructuredBuffer<TYPE> >::get()              \
{                                                                           \
    return g_StructuredBuffer_##TYPE[resourceHandle];                       \
}

//this needs some special care because of the wrapper type
#define ENABLE_CONSTANT_ACCESS(TYPE)                                        \
DECLARE_TEMPLATED_ARRAY(ConstantBuffer, TYPE, UNIFORM_BUFFER_SET, 0)        \
template <>                                                                 \
ConstantBuffer_fix<TYPE> Handle< ConstantBuffer_fix<TYPE> >::get()          \
{                                                                           \
    return (ConstantBuffer_fix<TYPE>)(resourceHandle);                      \
}                                                                           \
template <>                                                                 \
TYPE ConstantBuffer_fix<TYPE>::Load()                                       \
{                                                                           \
    return g_ConstantBuffer_##TYPE[resourceHandle];                         \
};                                                                          \

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