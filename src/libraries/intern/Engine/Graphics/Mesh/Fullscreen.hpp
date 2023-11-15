#pragma once

#include "Mesh.hpp"
#include <vector>

/*
    See notes in Cube.hpp
*/

struct FullscreenTri
{
    static inline const std::vector<Mesh::PositionType> positions = {
        {-1, -1, 0},
        {-1, 3, 0},
        {3, -1, 0},
    };

    static inline const std::vector<Mesh::BasicVertexAttributes<0>> attributes = {
        {.normal = {0.f, 0.f, 1.f}, .uvs = {{0.f, 0.f}}},
        {.normal = {0.f, 0.f, 1.f}, .uvs = {{0.f, 2.f}}},
        {.normal = {0.f, 0.f, 1.f}, .uvs = {{2.f, 0.f}}},
    };

    static inline const std::vector<Mesh::IndexType> indices = {
        0,
        1,
        2,
    };
};