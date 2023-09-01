#ifndef BINDLESS_SETUP_HLSL
#define BINDLESS_SETUP_HLSL

#include "Common.hlsl"
#include "Buffers.hlsl"
#include "Samplers.hlsl"
// todo: split into multiple files?

// ------------- Textures                                                            

#define DEFINE_DEFAULT_HANDLE_GETTER(TYPE, TEMPLATE)                        \
template <>                                                                 \
TYPE<TEMPLATE> Handle< TYPE<TEMPLATE> >::get()                              \
{                                                                           \
    return g_##TYPE##_##TEMPLATE[resourceHandle];                           \
}

#define ENABLE_SAMPLED_TEXTURE_ACCESS(TYPE, TEMPLATE)                               \
DECLARE_TEMPLATED_ARRAY(TYPE, TEMPLATE, SAMPLED_IMG_SET, GLOBAL_SAMPLER_COUNT)      \
DEFINE_DEFAULT_HANDLE_GETTER(TYPE, TEMPLATE)

#define ENABLE_STORAGE_TEXTURE_ACCESS(TYPE, TEMPLATE)           \
DECLARE_TEMPLATED_ARRAY(TYPE, TEMPLATE, STORAGE_IMG_SET, 0)     \
DEFINE_DEFAULT_HANDLE_GETTER(TYPE, TEMPLATE)

ENABLE_SAMPLED_TEXTURE_ACCESS(Texture2D, float2)
ENABLE_SAMPLED_TEXTURE_ACCESS(Texture2D, float4)
ENABLE_SAMPLED_TEXTURE_ACCESS(TextureCube, float4)

ENABLE_STORAGE_TEXTURE_ACCESS(RWTexture2D, float2)
ENABLE_STORAGE_TEXTURE_ACCESS(RWTexture2D, float4)
// There doesnt seem to be a RWTextureCube in HLSL, but seems like aliasing as Texture2DArray works
ENABLE_STORAGE_TEXTURE_ACCESS(RWTexture2DArray, float3)
ENABLE_STORAGE_TEXTURE_ACCESS(Texture2DArray, float4)
ENABLE_STORAGE_TEXTURE_ACCESS(RWTexture2DArray, float4)

// //This could be useful if I ever want to add format or other specifiers without breaking the abstraction
// //could use default params when format doesnt matter and specify if it does
// template<typename X, typename T = int>
// struct Foo
// {
//     ...
// };

// ------------- Input Bindings

// Not quite satisfied with this yet, prefer the one as shown by traverse:
//   have just this struct and then Load<T> from a byte address buffer
//       -> byte buffer doesnt need to know any types until user calls Load<T> supplying T
#define DefineShaderInputs(X)   \
struct ShaderInputs             \
{                               \
    X                           \
};                              \
[[vk::push_constant]]           \
ShaderInputs shaderInputs;

// currently 0th buffer is hardcoded (c++ side) to be the transform buffer
// static Handle< StructuredBuffer<float4x4> > globalTransformBuffer = Handle< StructuredBuffer<float4x4> >(0);
#define GLOBAL_TRANSFORMS g_StructuredBuffer_float4x4[0]

// --------------------------------

#endif