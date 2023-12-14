#ifndef FROM_CPP_HLSL
#define FROM_CPP_HLSL

#ifdef __cplusplus

using uint = uint32_t;
using float3 = glm::vec3;
using float2 = glm::vec2;
using float4x4 = glm::mat4;

template <typename T>
using StructuredBuffer = void;
using ByteAddressBuffer = void;

using Placeholder = void;

template <typename T>
using ResrcHandle = uint;

#endif

#endif