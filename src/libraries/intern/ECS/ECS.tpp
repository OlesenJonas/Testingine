#pragma once

#include "ECS.hpp"
#include <Datastructures/Span.hpp>
#include <cassert>

template <typename C>
void ECS::registerComponent()
{
    // Try to cache a component type id for C
    const ECSHelpers::TypeKey key = ECSHelpers::getTypeKey<C>();
    auto iter = componentTypeKeyToBitmaskIndexLUT.find(key);
    if(iter != componentTypeKeyToBitmaskIndexLUT.end())
    {
        // do nothing if component was already registered
        //  todo: warn?
        return;
    }

    uint32_t newBitmaskIndex = freeComponentBitmaskIndex++;
    componentTypeKeyToBitmaskIndexLUT.emplace(std::make_pair(key, newBitmaskIndex));

    if constexpr(ECSHelpers::is_trivially_relocatable<C>)
    {
        componentInfos[newBitmaskIndex] = {sizeof(C), nullptr, nullptr};
    }
    else
    {
        componentInfos[newBitmaskIndex] = {
            sizeof(C), ECSHelpers::moveConstructComponent<C>, ECSHelpers::destroyComponent<C>};
    }
}

template <typename... Types>
    requires(sizeof...(Types) >= 2) &&  //
            isDistinct<Types...>::value //
void ECS::forEach(std::function<void(std::add_pointer_t<Types>...)> func)
{
    ComponentMask componentMask;
    int i = 0;
    (
        [&]()
        {
            componentMask.set(bitmaskIndexFromComponentType<Types>());
            i++;
        }(),
        ... //
    );

    // run the func for all ements inside every archetype that has the required components
    for(auto& arch : archetypes)
    {
        if(arch.storageUsed == 0 || (arch.componentMask & componentMask) != componentMask)
            continue;

        for(int i = 0; i < arch.storageUsed; i++)
            func((                                                                                             //
                &((Types*)arch.componentArrays[arch.getArrayIndex(bitmaskIndexFromComponentType<Types>())])[i] //
                )...);
    }
};

template <typename Type>
void ECS::forEach(std::function<void(std::add_pointer_t<Type>)> func)
{
    // todo: could add a ECS::System type, that calculates this mask once on startup and stores it.
    //       then a system.run() could call ecs.forEach(system) or smth like that and just use the cached mask
    //       currently the mask and indices are calculated on every run !!!
    uint32_t bitmaskIndex = bitmaskIndexFromComponentType<Type>();
    ComponentMask combinedMask;
    combinedMask.set(bitmaskIndex);

    // run the func for all ements inside every archetype that has the required components
    for(auto& arch : archetypes)
    {
        if(arch.storageUsed == 0 || !arch.componentMask.test(bitmaskIndex))
            continue;

        for(int i = 0; i < arch.storageUsed; i++)
            func(&((Type*)arch.componentArrays[arch.getArrayIndex(bitmaskIndex)])[i]);
    }
};

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
C* ECS::addComponent(EntityID entityID, Args&&... args)
{
    auto archetypeEntryIt = entityLUT.find(entityID);
    assert(archetypeEntryIt != entityLUT.end());
    // instead of asserting false, could also just register the component now...
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
    Archetype* newArchetype = &archetypes[newArchetypeIndex];
    assert(newArchetype->componentMask == newMask);

    // check if storage left in new archetype
    if(newArchetype->storageUsed >= newArchetype->storageCapacity)
    {
        newArchetype->growStorage();
    }

    C* newComponent = nullptr;

    // move components into new archetype
    const uint32_t entityIndexInOldArchetype = archEntry.inArrayIndex;
    const uint32_t entityIndexInNewArchetype = newArchetype->storageUsed++;

    // step through both archetpyes storage arrays and move over component if its part of both
    uint32_t currentComponentBitmaskIndex = newMask.find_first();

    uint32_t oldArchComponentArrayIndex = 0;
    uint32_t newArchComponentArrayIndex = 0;
    for(; newArchComponentArrayIndex < newArchetypeCompCount; newArchComponentArrayIndex++)
    {
        const auto& componentInfo = componentInfos[currentComponentBitmaskIndex];

        // Component existed in old archetype
        if(oldMask[currentComponentBitmaskIndex])
        {
            auto* oldArchCompArray = (std::byte*)(oldArchetype->componentArrays[oldArchComponentArrayIndex]);
            auto* newArchCompArray = (std::byte*)(newArchetype->componentArrays[newArchComponentArrayIndex]);

            const ComponentInfo::MoveConstrFunc_t moveConstrFunc = componentInfo.moveConstrFunc;
            const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
            if(moveConstrFunc == nullptr)
            {
                // type is trivially relocatable, just memcpy component into new array
                memcpy(
                    &newArchCompArray[entityIndexInNewArchetype * componentInfo.size],
                    &oldArchCompArray[entityIndexInOldArchetype * componentInfo.size],
                    componentInfo.size);
            }
            else
            {
                // move component from old archetype into new archetype
                moveConstrFunc(
                    &oldArchCompArray[entityIndexInOldArchetype * componentInfo.size],
                    &newArchCompArray[entityIndexInNewArchetype * componentInfo.size]);
            }
            // also advance index in old archetypes component arrays
            oldArchComponentArrayIndex++;
        }
        else
        {
            // Component did not exist in old archetype
            // so this must be the one being added
            C* newArchCompStorageArray = (C*)(newArchetype->componentArrays[newArchComponentArrayIndex]);
            // this assumes the memory is "in destroyed state" (not sure about correct terminology)
            // -> construct instead of move into
            newComponent = new(&newArchCompStorageArray[entityIndexInNewArchetype]) C(std::forward<Args>(args)...);
        }
        currentComponentBitmaskIndex = newMask.find_next(currentComponentBitmaskIndex);
    }
    newArchetype->entityIDs.push_back(entityID);
    const ArchetypeEntry updatedEntry{
        .archetypeIndex = newArchetypeIndex, .inArrayIndex = entityIndexInNewArchetype};
    entityLUT[entityID] = updatedEntry;

    oldArchetype->removeEntry(entityIndexInOldArchetype);

    return newComponent;
}

template <typename C>
void ECS::removeComponent(EntityID entityID)
{
    auto archetypeEntryIt = entityLUT.find(entityID);
    assert(archetypeEntryIt != entityLUT.end());
    ArchetypeEntry archEntry = archetypeEntryIt->second;
    assert(archetypes[archEntry.archetypeIndex].entityIDs[archEntry.inArrayIndex] == entityID);

    Archetype* oldArchetype = &archetypes[archEntry.archetypeIndex];
    const ComponentMask oldMask = oldArchetype->componentMask;
    uint32_t componentTypeBitmaskIndex = bitmaskIndexFromComponentType<C>();
    if(!oldMask[componentTypeBitmaskIndex])
    {
        // TODO: warn "Object doesnt contain component of that type!");
        return;
    }

    ComponentMask newMask = oldMask;
    newMask.reset(componentTypeBitmaskIndex);
    assert(!newMask[componentTypeBitmaskIndex]);

    const uint32_t oldArchetypeCompCount = oldMask.count();

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
    Archetype* newArchetype = &archetypes[newArchetypeIndex];
    assert(newArchetype->componentMask == newMask);

    // check if storage left in new archetype
    if(newArchetype->storageUsed >= newArchetype->storageCapacity)
    {
        newArchetype->growStorage();
    }

    // move components into new archetype
    const uint32_t entityIndexInOldArchetype = archEntry.inArrayIndex;
    const uint32_t entityIndexInNewArchetype = newArchetype->storageUsed++;

    // step through both archetpyes storage arrays and move over component if its part of both
    uint32_t currentComponentBitmaskIndex = oldMask.find_first();

    uint32_t newArchComponentArrayIndex = 0;
    uint32_t oldArchComponentArrayIndex = 0;
    for(; oldArchComponentArrayIndex < oldArchetypeCompCount; oldArchComponentArrayIndex++)
    {
        const auto& componentInfo = componentInfos[currentComponentBitmaskIndex];

        auto* oldArchCompArray = (std::byte*)(oldArchetype->componentArrays[oldArchComponentArrayIndex]);

        const ComponentInfo::MoveConstrFunc_t moveConstrFunc = componentInfo.moveConstrFunc;
        const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;

        if(newMask[currentComponentBitmaskIndex])
        {
            // Component still exists in new archetype
            auto* newArchCompArray = (std::byte*)(newArchetype->componentArrays[newArchComponentArrayIndex]);

            if(moveConstrFunc == nullptr)
            {
                // type is trivially relocatable, just memcpy components into new array
                memcpy(
                    &newArchCompArray[entityIndexInNewArchetype * componentInfo.size],
                    &oldArchCompArray[entityIndexInOldArchetype * componentInfo.size],
                    componentInfo.size);
            }
            else
            {
                moveConstrFunc(
                    &oldArchCompArray[entityIndexInOldArchetype * componentInfo.size],
                    &newArchCompArray[entityIndexInNewArchetype * componentInfo.size]);
            }

            newArchComponentArrayIndex++;
        }
        currentComponentBitmaskIndex = oldMask.find_next(currentComponentBitmaskIndex);
    }
    newArchetype->entityIDs.push_back(entityID);
    const ArchetypeEntry updatedEntry{
        .archetypeIndex = newArchetypeIndex, .inArrayIndex = entityIndexInNewArchetype};
    entityLUT[entityID] = updatedEntry;

    oldArchetype->removeEntry(entityIndexInOldArchetype);
}

template <typename C>
C* ECS::getComponent(EntityID entityID)
{
    ArchetypeEntry& entry = entityLUT.find(entityID)->second;
    Archetype& archetype = archetypes[entry.archetypeIndex];
    uint32_t componentTypeBitmaskIndex = bitmaskIndexFromComponentType<C>();
    if(!archetype.componentMask[componentTypeBitmaskIndex])
    {
        // todo: warn user, trying to retrieve component that entitiy doesnt hold
        return nullptr;
    }
    uint32_t arrayIndex = archetype.getArrayIndex(componentTypeBitmaskIndex);
    assert(entry.inArrayIndex < archetype.storageUsed);
    C* storageArray = reinterpret_cast<C*>(archetype.componentArrays[arrayIndex]);
    return &(storageArray[entry.inArrayIndex]);
}