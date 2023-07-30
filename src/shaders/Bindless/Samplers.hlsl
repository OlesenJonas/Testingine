#ifndef BINDLESS_SAMPLERS_HLSL
#define BINDLESS_SAMPLERS_HLSL

#include "Common.hlsl"

// DONT change these unless the default initialization in Engine.cpp has changed
#define DEFAULT_SAMPLER_LINEAR_REPEAT 0
#define DEFAULT_SAMPLER_LINEAR_CLAMP 1
#define DEFAULT_SAMPLER_NEAREST 2

// ------------- Samplers

// Just one type of SamplerState, so this is done "manually"
[[vk::binding(0, SAMPLED_IMG_SET)]]
SamplerState g_samplerState[GLOBAL_SAMPLER_COUNT];
template<>
SamplerState Handle<SamplerState>::get()
{
    return g_samplerState[resourceHandle];
}

#define LinearRepeatSampler g_samplerState[DEFAULT_SAMPLER_LINEAR_REPEAT]
#define LinearClampSampler g_samplerState[DEFAULT_SAMPLER_LINEAR_CLAMP]
#define NearestRepeatSampler g_samplerState[DEFAULT_SAMPLER_NEAREST]

#endif