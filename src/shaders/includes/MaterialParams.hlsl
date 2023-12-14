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
    return ((ResrcHandle< ConstantBuffer<MaterialParameters> >)instanceInfo.materialParamsBuffer.resourceIndex).get();  \
}

#define MaterialInstanceParameters(members)                                                                                         \
struct MaterialInstanceParameters                                                                                                   \
{                                                                                                                                   \
    members                                                                                                                         \
};                                                                                                                                  \
ENABLE_CONSTANT_ACCESS(MaterialInstanceParameters)                                                                                  \
ConstantBuffer<MaterialInstanceParameters> getMaterialInstanceParameters(InstanceInfo instanceInfo)                                 \
{                                                                                                                                   \
    return ((ResrcHandle< ConstantBuffer<MaterialInstanceParameters> >)instanceInfo.materialInstanceParamsBuffer.resourceIndex).get();  \
}

#endif