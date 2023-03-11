#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform SceneData
{
    vec4 fogColor;
    vec4 fogDistance;
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
} sceneData;

layout (set=2, binding = 0) uniform sampler2D tex1;

void main()
{
    // fragColor = vec4(inColor + sceneData.ambientColor.xyz, 1.0);
    // fragColor = vec4(texCoord, 0.5, 1.0);
    vec3 color = texture(tex1, texCoord).xyz;
    fragColor = vec4(color, 1.0);
}