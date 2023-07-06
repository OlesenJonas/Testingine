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

    T get();
};

#endif 