#ifndef BINDLESS_GLSL
#define BINDLESS_GLSL

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

// most of this needs to match the c++ definitions
//  todo: include this header on c++ side aswell, so it does automatically
//        hide all the shader specifics with #ifdef __cplusplus

#define GLOBAL_SAMPLER_COUNT 32

#define SAMPLED_IMG_SET 0
#define STORAGE_IMG_SET 1
#define UBO_SET 2
#define SSBO_SET 3

struct BufferHandle
{
    uint index;
};
struct TextureHandle
{
    uint index;
};
struct SamplerHandle
{
    uint index;
};

//todo: switch to HLSL? Could really use templates, ByteAddressBuffers and especially operator overloading

//---------------- Buffers
#define _StorageBuffer(name, qualifier, contents) \
layout(std430, set=SSBO_SET, binding = 0) qualifier buffer name ## Buffer { contents } global_buffers_##name[]

#define StorageBuffer(name, contents) _StorageBuffer(name, , contents)
#define ReadStorageBuffer(name, contents) _StorageBuffer(name, readonly, contents)
#define WriteStorageBuffer(name, contents) _StorageBuffer(name, writeonly, contents)

#define UniformBuffer(name, contents) \
layout(std430, set=UBO_SET, binding = 0) uniform name ## Buffer { contents } global_buffers_##name[]

#define getBuffer(type, BufferHandle) global_buffers_##type[BufferHandle.index]

//---- Material Parameters (special case of UBO)
#define MaterialParameters(content) \
layout(std430, set=UBO_SET, binding = 0) uniform MaterialParametersBuffer {content} globalMaterialParametersBuffers[]

#define getMaterialParams(BufferHandle) globalMaterialParametersBuffers[BufferHandle.index]

//---------------- Textures
layout (set = SAMPLED_IMG_SET, binding = 0) uniform sampler Samplers[GLOBAL_SAMPLER_COUNT];

layout (set = SAMPLED_IMG_SET, binding = GLOBAL_SAMPLER_COUNT) uniform texture2D global_texture2D[];

#define Texture2D(TextureHandle) global_texture2D[TextureHandle.index]

//----------------

struct BindlessIndices
{
    // Frame globals
    BufferHandle FrameDataBuffer;
    // Resolution, matrices (differs in eg. shadow and default pass)
    BufferHandle RenderInfoBuffer;
    // Buffer with object transforms and index into that buffer
    BufferHandle transformBuffer;
    uint transformIndex;
    // Buffer with material/-instance parameters
    BufferHandle materialParamsBuffer;
    BufferHandle materialInstanceParamsBuffer;
};

#endif