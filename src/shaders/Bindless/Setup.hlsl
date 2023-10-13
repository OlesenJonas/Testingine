#ifndef BINDLESS_SETUP_HLSL
#define BINDLESS_SETUP_HLSL

#include "Common.hlsl"
#include "Buffers.hlsl"
#include "Textures.hlsl"
#include "Samplers.hlsl"

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

#endif