// move into bindless header -----

#define GLOBAL_SAMPLER_COUNT 32

#define SAMPLED_IMG_SET 0
#define STORAGE_IMG_SET 1
#define UBO_SET 2
#define SSBO_SET 3

/*
[[vk::binding(0, 0)]]
ByteAddressBuffer g_byteAddressBuffer[];
[[vk::binding(0, 0)]]
RWByteAddressBuffer g_rwByteAddressBuffer[];
*/

/*

// One veryyyy long but, when done correctly, probably fancy way of doing bindless
// Could be identical to standard HLSL syntax but with minor name changes
// Haven't investigated what the code-gen is like though

template <typename T>
struct RWTex2DArray;

template <>
struct RWTex2DArray<float4>
{
    uint resourceHandle;

    void GetDimensions(out uint Width, out uint Height, out uint Elements)
    {
    }
};

template <typename T>
struct Tex2D;

template<>
struct Tex2D<float4>
{
    uint resourceHandle;

    void foo();
};
*/

template<typename T>
struct Handle
{
    uint resourceHandle;

    T get();
};

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

#define DECLARE_TEMPLATED_TYPE(TYPE, TEMPLATE, DESCR, BINDING)  \
[[vk::binding(BINDING, DESCR)]]                                 \
TYPE<TEMPLATE> g_##TYPE##_##TEMPLATE[];                         \
template <>                                                     \
TYPE<TEMPLATE> Handle< TYPE<TEMPLATE> >::get()                  \
{                                                               \
    return g_##TYPE##_##TEMPLATE[resourceHandle];               \
}                                                               \

DECLARE_TEMPLATED_TYPE(Texture2D, float4, SAMPLED_IMG_SET, GLOBAL_SAMPLER_COUNT)

// There doesnt seem to be a RWTextureCube in HLSL, but seems like aliasing as Texture2DArray works
DECLARE_TEMPLATED_TYPE(RWTexture2DArray, float4, STORAGE_IMG_SET, 0)

#undef DECLARE_TEMPLATED_TYPE

// ------------- Buffers

// TODO:

/*

//This could be useful if I ever want to add format or other specifiers without breaking the abstraction
//could use default params when format doesnt matter and specify if it does
template<typename X, typename T = int>
struct Foo
{
    ...
};

*/

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

DefineShaderInputs(
    Handle< Texture2D<float4> > sourceTex;
    Handle< SamplerState > sourceSampler;
    Handle< RWTexture2DArray<float4> > targetTex;
);

static const float2 invAtan = float2(0.1591, 0.3183);
float2 sampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), -asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

[numthreads(8, 8, 1)]
void main(
    uint3 GlobalInvocationID : SV_DispatchThreadID,
    uint3 LocalInvocationID : SV_GroupThreadID
)
{
    int3 id = GlobalInvocationID;

    int3 size;
    RWTexture2DArray<float4> targetTex = shaderInputs.targetTex.get();
    targetTex.GetDimensions(size.x, size.y, size.z);
    float2 uv = float2(id.xy + 0.5) / size.xy;
    uv.y = 1.0 - uv.y;
    uv = 2.0 * uv - 1.0;

    float3 localPos;
    if(id.z == 0)
        localPos = float3(1.0, uv.y, -uv.x); // pos x
    else if(id.z == 1)
        localPos = float3(-1.0, uv.y, uv.x); // neg x
    else if(id.z == 2)
        localPos = float3(uv.x, 1.0, -uv.y); // pos y
    else if(id.z == 3)
        localPos = float3(uv.x, -1.0, uv.y); // neg y
    else if(id.z == 4)
        localPos = float3(uv.x, uv.y, 1.0); // pos z
    else if(id.z == 5)
        localPos = float3(-uv.x, uv.y, -1.0); // neg z

    float3 color =
        shaderInputs.sourceTex.get().SampleLevel(
            shaderInputs.sourceSampler.get(),
            sampleSphericalMap(normalize(localPos)),
            0
        ).xyz;

    targetTex[id] = float4(color, 1.0);
}