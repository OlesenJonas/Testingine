#version 460

#extension GL_GOOGLE_include_directive : require

#include "Bindless.glsl"
#include "VertexAttributes.glsl"

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
    const mat4 modelMatrix = getBuffer(Transform, bindlessIndices.transformBuffer).modelMatrices[bindlessIndices.transformIndex];
    const mat4 transformMatrix = getBuffer(RenderPassData, bindlessIndices.renderPassDataBuffer).projView * modelMatrix;
    gl_Position = transformMatrix * vec4(vPosition, 1.0);
    outColor = vColor;
    texCoord = vTexCoord;
}