#version 460

#extension GL_GOOGLE_include_directive : require

/*
    WIP !
*/

#include "Bindless.glsl"

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 texCoord;

ReadStorageBuffer(Transform,
    mat4 modelMatrices[];
);

UniformBuffer(CameraData,
    mat4 view;
    mat4 proj;
    mat4 projView;
);

layout (push_constant, std430) uniform constants
{
    BindlessIndices bindlessIndices;
};

void main()
{
    mat4 modelMatrix = getBuffer(Transform, bindlessIndices.transformBuffer).modelMatrices[bindlessIndices.transformIndex];
    mat4 transformMatrix = getBuffer(CameraData, bindlessIndices.RenderInfoBuffer).projView * modelMatrix;
    gl_Position = transformMatrix * vec4(vPosition, 1.0);
    outColor = vColor;
    texCoord = vTexCoord;
}