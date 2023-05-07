#pragma once

#include <EASTL/bitset.h>
#include <cassert>
#include <cstdint>

using IDType = uint32_t;
using EntityID = IDType;
using ComponentTypeID = IDType;
// just here to prevent copy paste errors
static_assert(std::is_unsigned_v<EntityID> && std::is_unsigned_v<ComponentTypeID>);
constexpr uint32_t MAX_COMPONENT_TYPES = 64;
// not sure if making this a bitmask is actually a net win, some tests are faster, some slower
using ComponentMask = eastl::bitset<MAX_COMPONENT_TYPES>;
using ComponentMaskHash = eastl::hash<ComponentMask>;