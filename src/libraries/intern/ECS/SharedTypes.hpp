#pragma once

#include <EASTL/bitset.h>
#include <cassert>
#include <cstdint>

using IDType = uint32_t;
using EntityID = IDType;
using ComponentTypeID = IDType;
// just here to prevent copy paste errors
static_assert(std::is_unsigned_v<EntityID> && std::is_unsigned_v<ComponentTypeID>);
constexpr uint32_t MAX_COMPONENT_TYPES = 32;
// not sure if making this a bitmask is actually a net win, some tests are faster, some slower
using ComponentMask = eastl::bitset<MAX_COMPONENT_TYPES, uint64_t>;
// Hash function for ComponentMask
namespace std
{
    template <>
    struct hash<ComponentMask>
    {
        std::size_t operator()(const ComponentMask& key) const
        {
            static_assert(is_same_v<ComponentMask::word_type, uint64_t>);
            // eastl bitset guarantees that any bits > size are kept at 0, dont have to worry about that
            if constexpr(ComponentMask::kWordCount == 1)
            {
                return key.mWord[0];
            }
            else
            {
                uint64_t hash = 0;
                for(int i = 0; i < key.kWordCount; i++)
                {
                    hash ^= key.mWord[i];
                }
                return hash;
            }
        }
    };

} // namespace std