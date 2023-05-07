#pragma once

#include "ECS.hpp"
#include <intern/Misc/Concepts.hpp>

template <typename C>
void moveComponent(void* src, void* dst)
{
    C& dstC = *(C*)dst;
    dstC = std::move(*((C*)src));
}

template <typename C>
void destroyComponent(void* ptr)
{
    ((C*)ptr)->~C();
}

template <typename C>
void ECS::registerComponent()
{
    // Try to cache a component type id for C
    ComponentTypeID compTypeID = ComponentTypeIDGenerator::GetNewID<C>();

    if constexpr(is_trivially_relocatable<C>)
    {
        ComponentInfo info{sizeof(C), nullptr, nullptr};
        componentInfos.emplace_back(info);
    }
    else
    {
        componentInfos.emplace_back(sizeof(C), moveComponent<C>, destroyComponent<C>);
    }
}

template <typename C, typename... Args>
C& ECS::addComponent(Entity* entity, Args&&... args)
{
    auto archetypeEntryIt = entityLUT.find(entity->id);
    assert(archetypeEntryIt != entityLUT.end());
    ArchetypeEntry archEntry = archetypeEntryIt->second;

    Archetype* oldArchetype = &archetypes[archEntry.archetypeIndex];
    const ComponentMask& oldMask = oldArchetype->componentMask;
    ComponentTypeID cId = ComponentTypeIDCache<C>::getID();
    if(oldMask[cId])
    {
        assert(false && "Object already contains component of that type!");
    }

    ComponentMask mask = oldMask;
    mask.set(cId);

    const uint32_t newArchetypeCompCount = mask.count();

    Archetype* newArchetype = nullptr;
    auto newArchetypeIter = archetypeLUT.find(mask);
    // Create new archetype with needed components if it doesnt exist yet
    if(newArchetypeIter == archetypeLUT.end())
    {
        uint32_t newArchetypeIndex = createArchetype(mask);
        newArchetype = &archetypes[newArchetypeIndex];
    }
    else
    {
        newArchetype = &archetypes[newArchetypeIter->second];
    }

    // check if storage left in new archetype
    if(newArchetype->storageUsed >= newArchetype->storageCapacity)
    {
        newArchetype->growStorage();
    }

    // move into new archetype
    const uint32_t indexInOldArchetype = archEntry.inArrayIndex;
    const uint32_t indexInNewArchetype = newArchetype->storageUsed;
    uint32_t lastFind = mask.find_first();
    uint32_t oldArchArrayIndex = 0;
    for(uint32_t newArchArrayIndex = 0; newArchArrayIndex < newArchetypeCompCount; newArchArrayIndex++)
    {
        const auto componentTypeID = lastFind;
        const auto& componentInfo = ECS::get()->componentInfos[componentTypeID];

        // Component existed in old archetype
        if(oldMask[componentTypeID])
        {
            std::byte* oldArchArray = (std::byte*)(oldArchetype->componentArrays[oldArchArrayIndex]);
            std::byte* newArchArray = (std::byte*)(newArchetype->componentArrays[newArchArrayIndex]);

            const ComponentInfo::MoveFunc_t moveFunc = componentInfo.moveFunc;
            const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
            if(moveFunc == nullptr)
            {
                // type is trivially relocatable, just memcpy components into new array
                memcpy(
                    &newArchArray[indexInNewArchetype * componentInfo.size],
                    &oldArchArray[indexInOldArchetype * componentInfo.size],
                    componentInfo.size);
            }
            else
            {
                moveFunc(
                    &oldArchArray[indexInOldArchetype * componentInfo.size],
                    &newArchArray[indexInNewArchetype * componentInfo.size]);
                // dont destroy, will be moved into when hole in Archetype is fixed!
            }

            lastFind = mask.find_next(lastFind);
            oldArchArrayIndex++;
        }
        else
        {
            construct new C inside new Archetypes array
        }
    }
    newArchetype->storageUsed++;

    // todo: much stuff still left out, see notes
}

template <typename C>
void ECS::removeComponent(Entity* entity)
{
}

template <typename C>
C* ECS::getComponent(Entity* entity)
{
    ArchetypeEntry& entry = entityLUT.at(entity->id);
    Archetype& archetype = archetypes[entry.archetypeIndex];
    const int bitIndex = ComponentTypeIDCache<C>::getID();
    if(!archetype.componentMask[bitIndex])
        return nullptr;
    // shift away all bits (including C::getID one) and count how many remain, thats the index
    // into the component array array
    const int highBitsToEliminate = MAX_COMPONENT_TYPES - bitIndex;
    ComponentMask mask = archetype.componentMask;
    mask >>= highBitsToEliminate;
    const auto arrayIndex = mask.count();
    assert(entry.inArrayIndex < archetype.storageUsed);
    C* array = reinterpret_cast<C*>(archetype.componentArrays[arrayIndex]);
    return &(array[entry.inArrayIndex]);
}