#ifndef BINDLESS_SETUP_HLSL
#define BINDLESS_SETUP_HLSL

#include "Common.hlsl"
#include "Buffers.hlsl"

// todo: split into multiple files?

// ------------- Samplers

// Just one type of SamplerState, so this is done "manually"
[[vk::binding(0, SAMPLED_IMG_SET)]]
SamplerState g_samplerState[GLOBAL_SAMPLER_COUNT];
template<>
SamplerState Handle<SamplerState>::get()
{
    return g_samplerState[resourceHandle];
}

// ------------- Textures                                                            

DECLARE_TEMPLATED_TYPE(Texture2D, float4, SAMPLED_IMG_SET, GLOBAL_SAMPLER_COUNT)

// There doesnt seem to be a RWTextureCube in HLSL, but seems like aliasing as Texture2DArray works
DECLARE_TEMPLATED_TYPE(RWTexture2DArray, float4, STORAGE_IMG_SET, 0)

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


// --------------------------------

// Define bindless access to the predefined structs
#include "../CommonTypes.hlsl"

DEFINE_BUFFERS_FOR_TYPE(RenderPassData)

#endif