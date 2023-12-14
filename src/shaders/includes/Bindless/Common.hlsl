#ifndef BINDLESS_COMMON_HLSL
#define BINDLESS_COMMON_HLSL

#define GLOBAL_SAMPLER_COUNT 32

#define SAMPLED_IMG_SET 0
#define STORAGE_IMG_SET 1
#define UNIFORM_BUFFER_SET 2
#define STORAGE_BUFFER_SET 3

/*
    Since im using strongly types buffers, and not all structs may be defined
    in all shader stage files, this Placeholder is defined, enabling a neat way
    of just writing ResrcHandle<Placeholder> ...
*/
struct Placeholder{};

template<typename T>
struct ResrcHandle
{
    uint resourceIndex;

    T get()
    {
        return (T)0;
    }

    bool isNonNull() 
    {
        return resourceIndex != 0xFFFFFFFF;
    }
};

template<>
struct ResrcHandle<Placeholder>
{
    uint resourceIndex;
};

#define DECLARE_RESOURCE_ARRAY_TEMPLATED(TYPE, TEMPLATE, SET, BINDING)      \
[[vk::binding(BINDING, SET)]]                                               \
TYPE<TEMPLATE> g_##TYPE##_##TEMPLATE[];                         

#define IMPLEMENT_HANDLE_GETTER(TYPE, TEMPLATE)                             \
template <>                                                                 \
TYPE<TEMPLATE> ResrcHandle< TYPE<TEMPLATE> >::get()                              \
{                                                                           \
    return g_##TYPE##_##TEMPLATE[resourceIndex];                           \
}

#endif 