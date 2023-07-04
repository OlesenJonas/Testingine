struct PushConstants
{
    uint sourceTex;
    uint samplerIndex;
    uint targetTex;    
};
[[vk::push_constant]]
PushConstants constants;

//todo: can I do this:?
/*
[[vk::push_constant]]
struct PushConstants
{
    uint sourceTex;
    uint samplerIndex;
    uint targetTex;  
} constants;
*/

static const float2 invAtan = float2(0.1591, 0.3183);
float2 sampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), -asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

// move into bindless header -----

#define GLOBAL_SAMPLER_COUNT 32

#define SAMPLED_IMG_SET 0
#define STORAGE_IMG_SET 1
#define UBO_SET 2
#define SSBO_SET 3

[[vk::binding(0, SAMPLED_IMG_SET)]]
SamplerState g_samplerState[GLOBAL_SAMPLER_COUNT];

// #define BINDLESS_TEXTURE_2D_DECL(T)             \
//         [[vk::binding(GLOBAL_SAMPLER_COUNT, SAMPLED_IMG_SET)]] Texture2D<T> g_texture2d[]


// BINDLESS_TEXTURE_2D_DECL(float4);

// #define DEFINE_TEXTURE_TYPES_AND_FORMATS_SLOTS(textureType)                                        \
//     [[vk::binding(GLOBAL_SAMPLER_COUNT, SAMPLED_IMG_SET)]] textureType<float>                      \
//         g_##textureType##float[];                                                                  \
//     [[vk::binding(GLOBAL_SAMPLER_COUNT, SAMPLED_IMG_SET)]] textureType<float2>                     \
//         g_##textureType##float2[];                                                                 \
//     [[vk::binding(GLOBAL_SAMPLER_COUNT, SAMPLED_IMG_SET)]] textureType<float4>                     \
//         g_##textureType##float4[];

// DEFINE_TEXTURE_TYPES_AND_FORMATS_SLOTS(Texture2D);

[[vk::binding(GLOBAL_SAMPLER_COUNT, SAMPLED_IMG_SET)]]
Texture2D<float4> g_Texture2D_float4[];

//TODO: 
//  Make macro and take format as parameter!
[[vk::binding(0, STORAGE_IMG_SET)]]
// RWTexture2D<float4> g_RWTexture2D_float4[];
// There doesnt seem to be a RWTextureCube in HLSL, so praying that aliasing as texture array will work
RWTexture2DArray<float4> g_RWTexture2DArray_float4[];

/*
[[vk::binding(0, 0)]]
ByteAddressBuffer g_byteAddressBuffer[];
[[vk::binding(0, 0)]]
RWByteAddressBuffer g_rwByteAddressBuffer[];
*/

// layout (set = SAMPLED_IMG_SET, binding = GLOBAL_SAMPLER_COUNT) uniform texture2D global_texture2D[];
// layout (set = STORAGE_IMG_SET, binding = 0, rgba32f) uniform writeonly restrict imageCube global_rgba32f_imageCube[];


// --------------------------------

[numthreads(8, 8, 1)]
void main(
    uint3 GlobalInvocationID : SV_DispatchThreadID,
    uint3 LocalInvocationID : SV_GroupThreadID)
{
    int3 id = GlobalInvocationID;

    int3 size;
    g_RWTexture2DArray_float4[constants.targetTex].GetDimensions(size.x, size.y, size.z);
    float2 uv = float2(id.xy + 0.5)/size.xy;
    uv.y = 1.0-uv.y;
    uv = 2.0 * uv - 1.0;

    float3 localPos;
    if(id.z == 0)      localPos = float3(  1.0, uv.y, -uv.x); // pos x
    else if(id.z == 1) localPos = float3( -1.0, uv.y,  uv.x); // neg x
    else if(id.z == 2) localPos = float3( uv.x,  1.0, -uv.y); // pos y
    else if(id.z == 3) localPos = float3( uv.x, -1.0,  uv.y); // neg y
    else if(id.z == 4) localPos = float3( uv.x, uv.y,   1.0); // pos z
    else if(id.z == 5) localPos = float3(-uv.x, uv.y,  -1.0); // neg z

    float3 color = g_Texture2D_float4[constants.sourceTex].SampleLevel(
        g_samplerState[constants.samplerIndex],
        sampleSphericalMap(normalize(localPos)),
        0
    ).xyz;

    g_RWTexture2DArray_float4[constants.targetTex][id] = float4(color, 1.0);
}