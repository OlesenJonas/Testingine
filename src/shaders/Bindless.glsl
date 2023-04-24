#ifndef BINDLESS_GLSL
#define BINDLESS_GLSL

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

// most of this needs to match the c++ definitions
//  todo: include this header on c++ side aswell, so it does automatically
//        hide all the shader specifics with #ifdef __cplusplus

struct BindlessIndices
{
    // Frame globals
    uint FrameDataBuffer;
    // Resolution, matrices (differs in eg. shadow and default pass)
    uint RenderInfoBuffer;
    // Buffer with object transforms and index into that buffer
    uint transformBuffer;
    uint transformIndex;
    // Buffer with material/-instance parameters
    uint materialParamsBuffer;
    uint materialInstanceParamsBuffer;
};

#define GLOBAL_SAMPLER_COUNT 1

#define SAMPLED_IMG_SET 0
#define STORAGE_IMG_SET 1
#define UBO_SET 2
#define SSBO_SET 3

#endif