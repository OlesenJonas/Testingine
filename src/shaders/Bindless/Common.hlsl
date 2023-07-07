#ifndef BINDLESS_COMMON_HLSL
#define BINDLESS_COMMON_HLSL

#define GLOBAL_SAMPLER_COUNT 32

#define SAMPLED_IMG_SET 0
#define STORAGE_IMG_SET 1
#define UNIFORM_BUFFER_SET 2
#define STORAGE_BUFFER_SET 3

template<typename T>
struct Handle
{
    uint resourceHandle;

    T get()
    {
        return (T)0;
    }
};

/*
    Since im using strongly types buffers, and not all structs may be defined
    in all shader stage files, this Placeholder is defined, enabling a neat way
    of just writing Handle<Placeholder> ...
*/
struct Placeholder{};
template<>
struct Handle<Placeholder>
{
    uint resourceHandle;

    Placeholder get()
    {
        return (Placeholder)0;
    }
};

#define DECLARE_TEMPLATED_ARRAY(TYPE, TEMPLATE, SET, BINDING)   \
[[vk::binding(BINDING, SET)]]                                   \
TYPE<TEMPLATE> g_##TYPE##_##TEMPLATE[];                         \

#endif 