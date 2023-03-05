#version 450

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;

layout (location = 0) out vec3 outColor;

layout (set = 0, binding = 0) uniform CameraBuffer
{
    mat4 view;
    mat4 proj;
    mat4 projView;
} cameraData;

layout (push_constant) uniform constants
{
    vec4 data;
    mat4 transformMatrix;
} PushConstants;

void main()
{
    mat4 transformMatrix = cameraData.projView * PushConstants.transformMatrix;
    gl_Position = transformMatrix * vec4(vPosition, 1.0);
    outColor = vColor;
}