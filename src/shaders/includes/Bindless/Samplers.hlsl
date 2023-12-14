#ifndef BINDLESS_SAMPLERS_HLSL
#define BINDLESS_SAMPLERS_HLSL

#include "Common.hlsl"

// ------------- Samplers

// Just one type of SamplerState, so this is done "manually"
[[vk::binding(0, SAMPLED_IMG_SET)]]
SamplerState g_samplerState[GLOBAL_SAMPLER_COUNT];
template<>
SamplerState Handle<SamplerState>::get()
{
    return g_samplerState[resourceIndex];
}

#include "DefaultSamplers.hlsl"
#define LinearRepeatSampler g_samplerState[DEFAULT_SAMPLER_LINEAR_REPEAT]
#define LinearClampSampler g_samplerState[DEFAULT_SAMPLER_LINEAR_CLAMP]
#define NearestRepeatSampler g_samplerState[DEFAULT_SAMPLER_NEAREST_CLAMP]

#endif