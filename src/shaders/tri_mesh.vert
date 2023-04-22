#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 texCoord;

#define SSBO_SET 3
layout(std430, set=SSBO_SET, binding = 0) readonly buffer TransformsBuffer { mat4 modelMatrices[]; } global_buffers_transforms[];
#define UBO_SET 2
layout (set = UBO_SET, binding = 0) uniform CameraBuffer{ mat4 view; mat4 proj; mat4 projView; } global_buffers_camera[];

layout (push_constant) uniform constants
{
    uint cameraBufferIndex;
    uint transformBufferIndex;
    uint transformIndex;
} bindlessIndices;

void main()
{
    mat4 modelMatrix = global_buffers_transforms[bindlessIndices.transformBufferIndex].modelMatrices[bindlessIndices.transformIndex];
    mat4 transformMatrix = global_buffers_camera[bindlessIndices.cameraBufferIndex].projView * modelMatrix;
    gl_Position = transformMatrix * vec4(vPosition, 1.0);
    outColor = vColor;
    texCoord = vTexCoord;
}