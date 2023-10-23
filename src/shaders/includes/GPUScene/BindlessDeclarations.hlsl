#ifndef GPUSCENE_BINDLESS_DECLS
#define GPUSCENE_BINDLESS_DECLS

#include "Structs.hlsl"
#include "../Bindless/Buffers.hlsl"

ENABLE_BINDLESS_BUFFER_ACCESS(MeshData);
ENABLE_BINDLESS_BUFFER_ACCESS(InstanceInfo);

#include "../CommonTypes.hlsl"
ENABLE_BINDLESS_BUFFER_ACCESS(RenderPassData)

ENABLE_BINDLESS_BUFFER_ACCESS(VertexAttributes)

#endif