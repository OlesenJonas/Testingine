#pragma once

#include "ParameterBuffer.hpp"

struct Material;

struct MaterialInstance
{
    Handle<Material> parentMaterial;
    ParameterBuffer parameters;
};

static_assert(is_trivially_relocatable<MaterialInstance>);