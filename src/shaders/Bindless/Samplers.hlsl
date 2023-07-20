#ifndef BINDLESS_SAMPLERS_HLSL
#define BINDLESS_SAMPLERS_HLSL

#include "Common.hlsl"

// DONT change these unless the default initialization in Engine.cpp has changed
#define DEFAULT_SAMPLER_LINEAR 0
#define DEFAULT_SAMPLER_NEAREST 1

// ------------- Samplers

// Just one type of SamplerState, so this is done "manually"
[[vk::binding(0, SAMPLED_IMG_SET)]]
SamplerState g_samplerState[GLOBAL_SAMPLER_COUNT];
template<>
SamplerState Handle<SamplerState>::get()
{
    return g_samplerState[resourceHandle];
}

#define LinearRepeatSampler g_samplerState[DEFAULT_SAMPLER_LINEAR]
#define NearestRepeatSampler g_samplerState[DEFAULT_SAMPLER_LINEAR]

#endif