#pragma once

#include "ParameterBuffer.hpp"

struct Material;

struct MaterialInstance
{
    Handle<Material> parentMaterial;
    ParameterBuffer parameters;
};

static_assert(PoolHelper::is_trivially_relocatable<MaterialInstance>);