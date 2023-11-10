#ifndef VERTEX_ATTR_HLSL
#define VERTEX_ATTR_HLSL

/*
    Since this is the same for all shaders atm and they all
    need to match a single layout defined on the application side
    its defined here
*/

struct VertexAttributes
{
    float3 normal;
    float3 color;
    float2 uv;
};

// struct VSInput
// {
//     [[vk::location(0)]] float3 vPosition : POSITION0;
//     [[vk::location(1)]] float3 vNormal : NORMAL0;
//     [[vk::location(2)]] float3 vColor : COLOR0;
//     [[vk::location(3)]] float2 vTexCoord : TEXCOORD0;
//     [[vk::builtin("BaseInstance")]]
//     int baseInstance : BASE_INSTANCE;
//     uint vertexID : SV_VertexID;
// };

struct VSInput
{
    uint vertexID : SV_VertexID;
    [[vk::builtin("BaseInstance")]]
    int baseInstance : BASE_INSTANCE;
};

#endif