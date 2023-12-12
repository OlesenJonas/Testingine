#ifndef GPUSCENE_PUSHCONSTANTS_HLSL
#define GPUSCENE_PUSHCONSTANTS_HLSL

#include "Structs.hlsl"
#include "../CommonTypes.hlsl"

#ifndef NO_DEFAULT_PUSH_CONSTANTS
struct DefaultPushConstants
{
    // Resolution, matrices (differs in eg. shadow and default pass)
    Handle< ConstantBuffer<RenderPassData> > renderPassData;
    // Buffer with information about all instances that are being rendered
    Handle< StructuredBuffer<InstanceInfo> > instanceBuffer;

    uint indexInInstanceBuffer;
};

[[vk::push_constant]]
DefaultPushConstants pushConstants;
#endif

#endif