#pragma once

#include <cstdint>
#include <glm/glm.hpp>

/*
 * returns the (up) rounded result of x/y without using floating point math
 */
inline uint32_t UintDivAndCeil(uint32_t x, uint32_t y) // NOLINT
{
    return (x + y - 1) / y;
}

inline glm::vec3 posFromPolar(float theta, float phi)
{
    return {sin(theta) * sin(phi), cos(theta), sin(theta) * cos(phi)};
}