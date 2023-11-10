#pragma once

#include "Mesh.hpp"
#include <vector>

/*
    Not sure which kind of API I prefer here...
    Current options:
        a) the current solution:
            give access to vertex positions and attributes and let user create Mesh
        b) Expose a create() function:
            the struct performs the createMesh() call internally and just retuns a handle directly
        c) ... ?
*/

struct Cube
{
    static inline constexpr float radius = 1.0f;
    // clang-format off
    static inline const std::vector<Mesh::PositionType> positions = {
        // +Z face
        {-radius, -radius, radius},
        { radius, -radius, radius},
        { radius,  radius, radius},
        {-radius,  radius, radius},
        // -Z face
        { radius, -radius,-radius},
        {-radius, -radius,-radius},
        {-radius,  radius,-radius},
        { radius,  radius,-radius},
        // +X face
        { radius, -radius, radius},
        { radius, -radius,-radius},
        { radius,  radius,-radius},
        { radius,  radius, radius},
        // -X face
        {-radius, -radius,-radius},
        {-radius, -radius, radius},
        {-radius,  radius, radius},
        {-radius,  radius,-radius},
        // +Y face
        {-radius,  radius, radius},
        { radius,  radius, radius},
        { radius,  radius,-radius},
        {-radius,  radius,-radius},
        // -Y face
        {-radius, -radius,-radius},
        { radius, -radius,-radius},
        { radius, -radius, radius},
        {-radius, -radius, radius},
    };

    static inline const std::vector<Mesh::VertexAttributes> attributes = {
        // +Z face
        {.normal = {0.f, 0.f, 1.f}, .uv = {0.f, 0.f}},
        {.normal = {0.f, 0.f, 1.f}, .uv = {1.f, 0.f}},
        {.normal = {0.f, 0.f, 1.f}, .uv = {1.f, 1.f}},
        {.normal = {0.f, 0.f, 1.f}, .uv = {0.f, 1.f}},
        // -Z face
        {.normal = {0.f, 0.f,-1.f}, .uv = {0.f, 0.f}},
        {.normal = {0.f, 0.f,-1.f}, .uv = {1.f, 0.f}},
        {.normal = {0.f, 0.f,-1.f}, .uv = {1.f, 1.f}},
        {.normal = {0.f, 0.f,-1.f}, .uv = {0.f, 1.f}},
        // +X face
        {.normal = {1.f, 0.f,0.f}, .uv = {0.f, 0.f}},
        {.normal = {1.f, 0.f,0.f}, .uv = {1.f, 0.f}},
        {.normal = {1.f, 0.f,0.f}, .uv = {1.f, 1.f}},
        {.normal = {1.f, 0.f,0.f}, .uv = {0.f, 1.f}},
        // -X face
        {.normal = {-1.f, 0.f,0.f}, .uv = {0.f, 0.f}},
        {.normal = {-1.f, 0.f,0.f}, .uv = {1.f, 0.f}},
        {.normal = {-1.f, 0.f,0.f}, .uv = {1.f, 1.f}},
        {.normal = {-1.f, 0.f,0.f}, .uv = {0.f, 1.f}},
        // +Y face
        {.normal = {0.f, 1.f,0.f}, .uv = {0.f, 0.f}},
        {.normal = {0.f, 1.f,0.f}, .uv = {1.f, 0.f}},
        {.normal = {0.f, 1.f,0.f}, .uv = {1.f, 1.f}},
        {.normal = {0.f, 1.f,0.f}, .uv = {0.f, 1.f}},
        // +Y face
        {.normal = {0.f,-1.f,0.f}, .uv = {0.f, 0.f}},
        {.normal = {0.f,-1.f,0.f}, .uv = {1.f, 0.f}},
        {.normal = {0.f,-1.f,0.f}, .uv = {1.f, 1.f}},
        {.normal = {0.f,-1.f,0.f}, .uv = {0.f, 1.f}},
    };
    
    static inline const std::vector<Mesh::IndexType> indices = {
        // +Z face
         0, 1, 2,     2, 3, 0,
        // -Z face
         4, 5, 6,     6, 7, 4,
        // +X face
         8, 9,10,    10,11, 8,
        // -X face
        12,13,14,    14,15,12,
        // +Y face
        16,17,18,    18,19,16,
        // -Y face
        20,21,22,    22,23,20,
    };
    // clang-format on
};