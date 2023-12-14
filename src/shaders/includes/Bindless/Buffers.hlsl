#ifndef BINDLESS_BUFFERS_HLSL
#define BINDLESS_BUFFERS_HLSL

#include "Common.hlsl"

// ---------------------

// ByteAddressBuffers are always treated as storage buffers :(

[[vk::binding(0, STORAGE_BUFFER_SET)]]                                   
ByteAddressBuffer g_ByteAddressBuffer[];

template<>
struct ResrcHandle< ByteAddressBuffer >
{
    uint resourceIndex;
    ByteAddressBuffer get()
    {
        return g_ByteAddressBuffer[resourceIndex];
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

#endif