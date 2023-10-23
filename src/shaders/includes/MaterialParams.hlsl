#ifndef MAT_PARAMS_HLSL
#define MAT_PARAMS_HLSL

#include "GPUScene/Structs.hlsl"

#define MaterialParameters(members)                                                                                 \
struct MaterialParameters                                                                                           \
{                                                                                                                   \
    members                                                                                                         \
};                                                                                                                  \
ENABLE_CONSTANT_ACCESS(MaterialParameters)                                                                          \
ConstantBuffer<MaterialParameters> getMaterialParameters(InstanceInfo instanceInfo)                                 \
{                                                                                                                   \
    return ((Handle< ConstantBuffer<MaterialParameters> >)instanceInfo.materialParamsBuffer.resourceHandle).get();  \
}

#define MaterialInstanceParameters(members)                                                                                         \
struct MaterialInstanceParameters                                                                                                   \
{                                                                                                                                   \
    members                                                                                                                         \
};                                                                                                                                  \
ENABLE_CONSTANT_ACCESS(MaterialInstanceParameters)                                                                                  \
ConstantBuffer<MaterialInstanceParameters> getMaterialInstanceParameters(InstanceInfo instanceInfo)                                 \
{                                                                                                                                   \
    return ((Handle< ConstantBuffer<MaterialInstanceParameters> >)instanceInfo.materialInstanceParamsBuffer.resourceHandle).get();  \
}

#endif