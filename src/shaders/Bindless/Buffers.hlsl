#ifndef BINDLESS_BUFFERS_HLSL
#define BINDLESS_BUFFERS_HLSL

#include "Common.hlsl"

/*
    Since im using strongly types buffers, and not all structs may be defined
    in all shader stage files, this Placeholder is defined, enabling a neat way
    of just writing Handle<Placeholder> ...
*/
struct Placeholder{};

#define StructForBindless(name, members) \
struct name     \
{               \
    members     \
};              \
DEFINE_BUFFERS_FOR_TYPE(name)

#define DEFINE_BUFFERS_FOR_TYPE(name) \
DECLARE_TEMPLATED_TYPE(StructuredBuffer, name, STORAGE_BUFFER_SET, 0)       \
DECLARE_TEMPLATED_TYPE(ConstantBuffer, name, UNIFORM_BUFFER_SET, 0)


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