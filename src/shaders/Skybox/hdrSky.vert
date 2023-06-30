#version 460

#extension GL_GOOGLE_include_directive : require

#include "../Bindless.glsl"
#include "../VertexAttributes.glsl"

layout (location = 0) out vec3 localPos;

ReadStorageBuffer(Transform,
    mat4 modelMatrices[];
);

layout (push_constant, std430) uniform constants
{
    BindlessIndices bindlessIndices;
};

void main()
{
    const mat4 modelMatrix = getBuffer(Transform, bindlessIndices.transformBuffer).modelMatrices[bindlessIndices.transformIndex];
    
    //mat4(mat3()) cast to remove camera translation!
    const mat4 transformMatrix = CurrentRenderPassData.proj * mat4(mat3(CurrentRenderPassData.view)) * modelMatrix;
    localPos = vPosition;
    //todo: dont just scale up by some large number, just make forcing depth to 1.0 work!
    gl_Position = transformMatrix * vec4(500*vPosition, 1.0);
}