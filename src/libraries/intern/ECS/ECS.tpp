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
C* ECS::addComponent(Entity* entity, Args&&... args)
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

    C* newComponent = nullptr;
    Archetype* newArchetype = nullptr;
    auto newArchetypeIter = archetypeLUT.find(mask);
    uint32_t newArchetypeIndex = 0xFFFFFFFF;
    // Create new archetype with needed components if it doesnt exist yet
    if(newArchetypeIter == archetypeLUT.end())
    {
        newArchetypeIndex = createArchetype(mask);
    }
    else
    {
        newArchetypeIndex = newArchetypeIter->second;
    }
    newArchetype = &archetypes[newArchetypeIndex];
    assert(newArchetype->componentMask == mask);

    // check if storage left in new archetype
    if(newArchetype->storageUsed >= newArchetype->storageCapacity)
    {
        newArchetype->growStorage();
    }

    // move into new archetype
    const uint32_t indexInOldArchetype = archEntry.inArrayIndex;
    const uint32_t indexInNewArchetype = newArchetype->storageUsed;
    newArchetype->storageUsed++;
    uint32_t lastFind = mask.find_first();
    uint32_t oldArchArrayIndex = 0;
    for(uint32_t newArchArrayIndex = 0; newArchArrayIndex < newArchetypeCompCount; newArchArrayIndex++)
    {
        const auto componentTypeID = lastFind;
        const auto& componentInfo = ECS::get()->componentInfos[componentTypeID];

        // Component existed in old archetype
        if(oldMask[componentTypeID])
        {
            std::byte* oldArchCompArray = (std::byte*)(oldArchetype->componentArrays[oldArchArrayIndex]);
            std::byte* newArchCompArray = (std::byte*)(newArchetype->componentArrays[newArchArrayIndex]);

            const ComponentInfo::MoveFunc_t moveFunc = componentInfo.moveFunc;
            const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
            if(moveFunc == nullptr)
            {
                // type is trivially relocatable, just memcpy components into new array
                memcpy(
                    &newArchCompArray[indexInNewArchetype * componentInfo.size],
                    &oldArchCompArray[indexInOldArchetype * componentInfo.size],
                    componentInfo.size);
            }
            else
            {
                moveFunc(
                    &oldArchCompArray[indexInOldArchetype * componentInfo.size],
                    &newArchCompArray[indexInNewArchetype * componentInfo.size]);
                // dont destroy, will be moved into when hole in Archetype is fixed!
            }

            lastFind = mask.find_next(lastFind);
            oldArchArrayIndex++;
        }
        else
        {
            // Component did not exist in old archetype
            // so this must be the one being added
            C* newArchCompArray = (C*)(newArchetype->componentArrays[newArchArrayIndex]);
            // this assumes the memory is "in destroyed state" (not sure about correct terminology)
            // -> construct instead of move into
            newComponent = new(&newArchCompArray[indexInNewArchetype]) C(std::forward<Args>(args)...);
        }
    }
    newArchetype->entityIDs.push_back(entity->id);
    const ArchetypeEntry updatedEntry{.archetypeIndex = newArchetypeIndex, .inArrayIndex = indexInNewArchetype};
    entityLUT[entity->id] = updatedEntry;

    oldArchetype->fixGap(indexInOldArchetype);

    return newComponent;
}

template <typename C>
void ECS::removeComponent(Entity* entity)
{
    auto archetypeEntryIt = entityLUT.find(entity->id);
    assert(archetypeEntryIt != entityLUT.end());
    ArchetypeEntry archEntry = archetypeEntryIt->second;

    Archetype* oldArchetype = &archetypes[archEntry.archetypeIndex];
    const ComponentMask& oldMask = oldArchetype->componentMask;
    ComponentTypeID cId = ComponentTypeIDCache<C>::getID();
    if(!oldMask[cId])
    {
        assert(false && "Object doesnt contain component of that type!");
    }

    ComponentMask mask = oldMask;
    mask.reset(cId);
    assert(!mask[cId]);

    const uint32_t oldArchetypeCompCount = oldMask.count();

    Archetype* newArchetype = nullptr;
    auto newArchetypeIter = archetypeLUT.find(mask);
    uint32_t newArchetypeIndex = 0xFFFFFFFF;
    // Create new archetype with needed components if it doesnt exist yet
    if(newArchetypeIter == archetypeLUT.end())
    {
        newArchetypeIndex = createArchetype(mask);
    }
    else
    {
        newArchetypeIndex = newArchetypeIter->second;
    }
    newArchetype = &archetypes[newArchetypeIndex];
    assert(newArchetype->componentMask == mask);

    // check if storage left in new archetype
    if(newArchetype->storageUsed >= newArchetype->storageCapacity)
    {
        newArchetype->growStorage();
    }

    // move into new archetype
    const uint32_t indexInOldArchetype = archEntry.inArrayIndex;
    const uint32_t indexInNewArchetype = newArchetype->storageUsed;
    newArchetype->storageUsed++;
    uint32_t lastFind = oldMask.find_first();
    uint32_t newArchArrayIndex = 0;
    for(uint32_t oldArchArrayIndex = 0; oldArchArrayIndex < oldArchetypeCompCount; oldArchArrayIndex++)
    {
        const auto componentTypeID = lastFind;
        const auto& componentInfo = ECS::get()->componentInfos[componentTypeID];

        if(mask[componentTypeID])
        {
            // Component still exists in new archetype
            std::byte* oldArchCompArray = (std::byte*)(oldArchetype->componentArrays[oldArchArrayIndex]);
            std::byte* newArchCompArray = (std::byte*)(newArchetype->componentArrays[newArchArrayIndex]);

            const ComponentInfo::MoveFunc_t moveFunc = componentInfo.moveFunc;
            const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
            if(moveFunc == nullptr)
            {
                // type is trivially relocatable, just memcpy components into new array
                memcpy(
                    &newArchCompArray[indexInNewArchetype * componentInfo.size],
                    &oldArchCompArray[indexInOldArchetype * componentInfo.size],
                    componentInfo.size);
            }
            else
            {
                moveFunc(
                    &oldArchCompArray[indexInOldArchetype * componentInfo.size],
                    &newArchCompArray[indexInNewArchetype * componentInfo.size]);
                // dont destroy, will be moved into when hole in Archetype is fixed!
            }

            lastFind = mask.find_next(lastFind);
            newArchArrayIndex++;
        }
        else
        {
            // Component does not exist in new archetype
            // so this must be the one being removed
            // -- do nothing --
        }
    }
    newArchetype->entityIDs.push_back(entity->id);
    const ArchetypeEntry updatedEntry{.archetypeIndex = newArchetypeIndex, .inArrayIndex = indexInNewArchetype};
    entityLUT[entity->id] = updatedEntry;

    oldArchetype->fixGap(indexInOldArchetype);
}

template <typename C>
C* ECS::getComponent(Entity* entity)
{
    ArchetypeEntry& entry = entityLUT.find(entity->id)->second;
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