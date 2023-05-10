#pragma once

#include "ECS.hpp"
#include <intern/Misc/Concepts.hpp>

template <typename C>
void ECS::registerComponent()
{
    // Try to cache a component type id for C
    // ECSHelpers::TypeKey
    const ECSHelpers::TypeKey key = ECSHelpers::getTypeKey<C>();
    auto iter = componentTypeKeyToBitmaskIndexLUT.find(key);
    assert(
        iter == componentTypeKeyToBitmaskIndexLUT.end() &&
        "Trying to register a component that has already been registered!");

    uint32_t newBitmaskIndex = bitmaskIndexCounter++;
    componentTypeKeyToBitmaskIndexLUT.emplace(std::make_pair(key, newBitmaskIndex));

    if constexpr(is_trivially_relocatable<C>)
    {
        ComponentInfo info{sizeof(C), nullptr, nullptr};
        componentInfos.emplace_back(info);
    }
    else
    {
        ComponentInfo info{sizeof(C), ECSHelpers::moveConstructComponent<C>, ECSHelpers::destroyComponent<C>};
        componentInfos.emplace_back(info);
    }
    assert(componentInfos.size() - 1 == newBitmaskIndex);
}

template <typename C>
uint32_t ECS::bitmaskIndexFromComponentType()
{
    const ECSHelpers::TypeKey key = ECSHelpers::getTypeKey<C>();
    auto iter = componentTypeKeyToBitmaskIndexLUT.find(key);
    assert(
        iter != componentTypeKeyToBitmaskIndexLUT.end() &&
        "Trying to find the index of a component that has not been registered yet!");
    return iter->second;
}

template <typename C, typename... Args>
C* ECS::addComponent(Entity* entity, Args&&... args)
{
    auto archetypeEntryIt = entityLUT.find(entity->id);
    assert(archetypeEntryIt != entityLUT.end());
    ArchetypeEntry archEntry = archetypeEntryIt->second;

    Archetype* oldArchetype = &archetypes[archEntry.archetypeIndex];
    const ComponentMask oldMask = oldArchetype->componentMask;
    uint32_t componentTypeBitmaskIndex = bitmaskIndexFromComponentType<C>();
    if(oldMask[componentTypeBitmaskIndex])
    {
        assert(false && "Object already contains component of that type!");
    }

    ComponentMask newMask = oldMask;
    newMask.set(componentTypeBitmaskIndex);

    const uint32_t newArchetypeCompCount = newMask.count();

    C* newComponent = nullptr;
    Archetype* newArchetype = nullptr;
    auto newArchetypeIter = archetypeLUT.find(newMask);
    uint32_t newArchetypeIndex = 0xFFFFFFFF;
    // Create new archetype with needed components if it doesnt exist yet
    if(newArchetypeIter == archetypeLUT.end())
    {
        newArchetypeIndex = createArchetype(newMask);
        // This can resize the archetypes array, so we need to refresh any Archetype variables retrieved earlier
        oldArchetype = &archetypes[archEntry.archetypeIndex];
    }
    else
    {
        newArchetypeIndex = newArchetypeIter->second;
    }
    newArchetype = &archetypes[newArchetypeIndex];
    assert(newArchetype->componentMask == newMask);

    // check if storage left in new archetype
    if(newArchetype->storageUsed >= newArchetype->storageCapacity)
    {
        newArchetype->growStorage();
    }

    // move into new archetype
    const uint32_t indexInOldArchetype = archEntry.inArrayIndex;
    const uint32_t indexInNewArchetype = newArchetype->storageUsed;
    newArchetype->storageUsed++;
    uint32_t lastFind = newMask.find_first();
    uint32_t oldArchArrayIndex = 0;
    for(uint32_t newArchArrayIndex = 0; newArchArrayIndex < newArchetypeCompCount; newArchArrayIndex++)
    {
        const auto componentTypeID = lastFind;
        const auto& componentInfo = componentInfos[componentTypeID];

        // Component existed in old archetype
        if(oldMask[componentTypeID])
        {
            std::byte* oldArchCompArray = (std::byte*)(oldArchetype->componentArrays[oldArchArrayIndex]);
            std::byte* newArchCompArray = (std::byte*)(newArchetype->componentArrays[newArchArrayIndex]);

            const ComponentInfo::MoveConstrFunc_t moveFunc = componentInfo.moveFunc;
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
                // move component from old archetype into new archetype, then destroy component left overs in the
                // old archetype
                moveFunc(
                    &oldArchCompArray[indexInOldArchetype * componentInfo.size],
                    &newArchCompArray[indexInNewArchetype * componentInfo.size]);
                destroyFunc(&oldArchCompArray[indexInOldArchetype * componentInfo.size]);
            }

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
        lastFind = newMask.find_next(lastFind);
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
    assert(archetypes[archEntry.archetypeIndex].entityIDs[archEntry.inArrayIndex] == entity->getID());

    Archetype* oldArchetype = &archetypes[archEntry.archetypeIndex];
    const ComponentMask oldMask = oldArchetype->componentMask;
    uint32_t componentTypeBitmaskIndex = bitmaskIndexFromComponentType<C>();
    if(!oldMask[componentTypeBitmaskIndex])
    {
        assert(false && "Object doesnt contain component of that type!");
    }

    ComponentMask newMask = oldMask;
    newMask.reset(componentTypeBitmaskIndex);
    assert(!newMask[componentTypeBitmaskIndex]);

    const uint32_t oldArchetypeCompCount = oldMask.count();

    Archetype* newArchetype = nullptr;
    auto newArchetypeIter = archetypeLUT.find(newMask);
    uint32_t newArchetypeIndex = 0xFFFFFFFF;
    // Create new archetype with needed components if it doesnt exist yet
    if(newArchetypeIter == archetypeLUT.end())
    {
        newArchetypeIndex = createArchetype(newMask);
        // This can resize the archetypes array, so we need to refresh any Archetype variables retrieved earlier
        oldArchetype = &archetypes[archEntry.archetypeIndex];
    }
    else
    {
        newArchetypeIndex = newArchetypeIter->second;
    }
    newArchetype = &archetypes[newArchetypeIndex];
    assert(newArchetype->componentMask == newMask);

    // check if storage left in new archetype (unless its the empty archetype)
    if(newMask.any() && newArchetype->storageUsed >= newArchetype->storageCapacity)
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
        const auto& componentInfo = componentInfos[componentTypeID];

        std::byte* oldArchCompArray = (std::byte*)(oldArchetype->componentArrays[oldArchArrayIndex]);

        const ComponentInfo::MoveConstrFunc_t moveFunc = componentInfo.moveFunc;
        const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;

        if(newMask[componentTypeID])
        {
            // Component still exists in new archetype
            std::byte* newArchCompArray = (std::byte*)(newArchetype->componentArrays[newArchArrayIndex]);

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
                destroyFunc(&oldArchCompArray[indexInOldArchetype * componentInfo.size]);
            }

            newArchArrayIndex++;
        }
        else
        {
            // Component does not exist in new archetype
            // so this must be the one being removed
            if(destroyFunc == nullptr)
            {
                // type is trivially relocatable, cant just leave memory for future free
            }
            else
            {
                destroyFunc(&oldArchCompArray[indexInOldArchetype * componentInfo.size]);
            }
        }
        lastFind = newMask.find_next(lastFind);
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
    uint32_t componentTypeBitmaskIndex = bitmaskIndexFromComponentType<C>();
    if(!archetype.componentMask[componentTypeBitmaskIndex])
        return nullptr;
    // shift away all bits (including C::getID one) and count how many remain, thats the index
    // into the component array array
    const int highBitsToEliminate = MAX_COMPONENT_TYPES - componentTypeBitmaskIndex;
    ComponentMask mask = archetype.componentMask;
    mask <<= highBitsToEliminate;
    const auto arrayIndex = mask.count();
    assert(entry.inArrayIndex < archetype.storageUsed);
    C* array = reinterpret_cast<C*>(archetype.componentArrays[arrayIndex]);
    return &(array[entry.inArrayIndex]);
}