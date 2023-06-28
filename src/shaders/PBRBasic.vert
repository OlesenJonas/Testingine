#version 460

#extension GL_GOOGLE_include_directive : require

/*
    WIP !
*/

#include "Bindless.glsl"
#include "VertexAttributes.glsl"

//todo: reorder, sort whatever
layout (location = 0) out vec3 vPositionWS;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 texCoord;
layout (location = 3) out vec3 vNormalWS;
layout (location = 4) out vec4 vTangentWS;

//todo: move into include file!
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
    const mat4 transformMatrix = getBuffer(RenderPassData, bindlessIndices.renderPassDataBuffer).projView * modelMatrix;
    gl_Position = transformMatrix * vec4(vPosition, 1.0);
    outColor = vColor;
    texCoord = vTexCoord;
    //dont think normalize is needed
    const mat3 modelMatrix3 = mat3(modelMatrix);
    //todo: pass transpose(inverse()) as part of upload buffer?
    vNormalWS = normalize( transpose(inverse(modelMatrix3)) * vNormal);
    vTangentWS = vec4(normalize( modelMatrix3 * vTangent.xyz),vTangent.w);
}