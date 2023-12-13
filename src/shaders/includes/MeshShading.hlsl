#ifndef MESH_SHADING_HLSL
#define MESH_SHADING_HLSL

//TODO: passing buffers in mesh shaders doesnt currently work (see Access.hlsl)
//      so its not possible to actually "include" this function, so use this as a base for copy pasting

uint3 extractPrimIndices(const int baseOffset, const uint primIndex /*, const StructuredBuffer<uint> meshletPrimitiveIndices*/)
{
    // When used with 32bit meshlet index buffer
    // uint3 primIndices;
    // primIndices[0] = meshletPrimitiveIndices[m.primBegin + primIndex*3 + 0];
    // primIndices[1] = meshletPrimitiveIndices[m.primBegin + primIndex*3 + 1];
    // primIndices[2] = meshletPrimitiveIndices[m.primBegin + primIndex*3 + 2];
    // tris[primIndex] = primIndices;

    //unpacking u8s from u32 array
    // uint3 u8indices = uint3(
    //     m.primBegin + primIndex*3 + 0,
    //     m.primBegin + primIndex*3 + 1,
    //     m.primBegin + primIndex*3 + 2
    // );

    // uint3 wordIndices = u8indices / 4u;
    // uint3 inWordOffset = u8indices % 4u;
    // uint3 asUints = uint3(
    //     meshletPrimitiveIndices[wordIndices[0]],
    //     meshletPrimitiveIndices[wordIndices[1]],
    //     meshletPrimitiveIndices[wordIndices[2]]
    // );
    // asUints &= 0xFFu << (inWordOffset * 8u);
    // asUints >>= (inWordOffset * 8u);
    // tris[primIndex] = asUints;

    // // // optimized version
    // // three u8s fit into max 2 words
    // const uint primBaseIndex = m.primBegin + primIndex*3u;
    // uint2 words = uint2(
    //     meshletPrimitiveIndices[(primBaseIndex + 0) / 4u],
    //     meshletPrimitiveIndices[(primBaseIndex + 2) / 4u]
    // );
    // const uint byteStartInWord0 = primBaseIndex % 4u;
    // uint bytesInWord0 = 4u - byteStartInWord0;
    // // move word0 into lower bits
    // uint combined = words[0] >> (byteStartInWord0 * 8u);
    // // mask unused bits
    // combined &= ~(0xFFFFFFFFu << (bytesInWord0 * 8u));
    // // move word1 into upper bits
    // combined |= words[1] << (bytesInWord0 * 8u);

    // tris[primIndex][0] = combined & 0xFFu;
    // tris[primIndex][1] = (combined >> 8u) & 0xFFu;
    // tris[primIndex][2] = (combined >> 16u) & 0xFFu;

    // further optimized version
    // const uint primBaseU8Index = m.primBegin + primIndex*3u;
    // uint word = meshletPrimitiveIndices[primBaseU8Index >> 2u];
    // //TODO: can do bitStartInWord0 instead ?!
    // const uint byteStartInWord0 = primBaseU8Index & 0b11u;
    // const uint bitsInWord0 = (4u - byteStartInWord0) << 3u;
    // // move relevant bytes into lowest bits
    // word >>= (byteStartInWord0 << 3u);
    // // mask unused bits
    // word &= ~(0xFFFFFFFFu << bitsInWord0);
    // // move relevant bytes from next word into upper bits
    // word |= meshletPrimitiveIndices[(primBaseU8Index + 2) >> 2u] << bitsInWord0;

    const uint primBaseU8Index = baseOffset + primIndex*3u;
    uint word = meshletPrimitiveIndices[primBaseU8Index >> 2u];
    const uint bitStartInWord0 = (primBaseU8Index << 3u) & 0b11111u;
    const uint bitsInWord0 = 32u - bitStartInWord0;
    // move relevant bytes into lowest bits
    word >>= bitStartInWord0;
    // mask unused bits
    word &= ~(0xFFFFFFFFu << bitsInWord0);
    // move relevant bytes from next word into upper bits
    word |= meshletPrimitiveIndices[(primBaseU8Index + 2) >> 2u] << bitsInWord0;

    return uint3(
        (word)          & 0xFFu,
        (word >> 8u)    & 0xFFu,
        (word >> 16u)   & 0xFFu
    );
}

#endif