#ifndef CULLING_HLSL
#define CULLING_HLSL

/*
    Does not return near or far planes
*/
void getFrustumPlanes(float4x4 mat, out float4 planes[6])
{
    const float4 row4 = float4(mat._41,mat._42,mat._43,mat._44);
    const float4 row1 = float4(mat._11,mat._12,mat._13,mat._14);
    const float4 row2 = float4(mat._21,mat._22,mat._23,mat._24);
    const float4 row3 = float4(mat._31,mat._32,mat._33,mat._34);
    
    // left
    planes[0] = row4 + row1;
    float length = sqrt(dot(planes[0].xyz, planes[0].xyz));
    planes[0] /= length;
    
    // right
    planes[1] = row4 - row1;
    length = sqrt(dot(planes[1].xyz, planes[1].xyz));
    planes[1] /= length;

    // bottom
    planes[2] = row4 + row2;
    length = sqrt(dot(planes[2], planes[2]));
    planes[2] /= length;

    // top
    planes[3] = row4 - row1;
    length = sqrt(dot(planes[3].xyz, planes[3].xyz));
    planes[3] /= length;

    // near
    planes[4] = row3;
    length = sqrt(dot(planes[4].xyz, planes[4].xyz));
    planes[4] /= length;

    // far
    planes[5] = row4 - row3;
    length = sqrt(dot(planes[5].xyz, planes[5].xyz));
    planes[5] /= length;
}

#endif