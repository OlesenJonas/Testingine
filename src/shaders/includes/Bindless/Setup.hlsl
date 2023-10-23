#ifndef BINDLESS_SETUP_HLSL
#define BINDLESS_SETUP_HLSL

#include "Common.hlsl"
#include "Buffers.hlsl"
#include "Textures.hlsl"
#include "Samplers.hlsl"
#include "DefaultDeclarations.hlsl"

#define DefinePushConstants(X)  \
struct PushConstants            \
{                               \
    X                           \
};                              \
[[vk::push_constant]]           \
PushConstants pushConstants;

#endif