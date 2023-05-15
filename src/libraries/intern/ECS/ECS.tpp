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

    uint32_t newBitmaskIndex = bitmaskIndexCounter++;
    componentTypeKeyToBitmaskIndexLUT.emplace(std::make_pair(key, newBitmaskIndex));

    if constexpr(ECSHelpers::is_trivially_relocatable<C>)
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

template <typename... Types, std::size_t... I>
void ECS::fillBitmaskAndIndices(
    ArrayReference<uint32_t, sizeof...(Types)> indices, ComponentMask& mask, std::index_sequence<I...>)
{
    //(ab-)using the fact that assignment returns the left operand to combine both operations
    //  In case I forget how this works: (f(Pack), ...) is a fold expression that expands to
    //  (f(Pack[0]),...,f(Pack[N]))
    (mask.set(indices[I] = bitmaskIndexFromComponentType<Types>()), ...);
}

template <typename... Types, std::size_t... I>
void ECS::fillPtrTuple(
    ArrayReference<uint32_t, sizeof...(Types)> indices,
    ECS::Archetype& archetype,
    std::tuple<std::add_pointer_t<Types>...>& tuple,
    std::index_sequence<I...>)
{
    ((std::get<I>(tuple) = (Types*)archetype.componentArrays[archetype.getArrayIndex(indices[I])]), ...);
}

template <typename... Types, typename Func>
    requires(sizeof...(Types) > 0) &&                                    //
            isDistinct<Types...>::value &&                               //
            std::invocable<Func, std::add_pointer_t<Types>..., uint32_t> //
void ECS::forEach(Func func)
{
    // todo: could add a ECS::System type, that calculates this mask once on startup and stores it.
    //       then a system.run() could call ecs.forEach(system) or smth like that and just use the cached mask
    //       currently the mask and indices are calculated on every run !!!
    // std::array<uint32_t, sizeof...(Types)> bitmaskIndices;
    uint32_t bitmaskIndices[sizeof...(Types)];
    ComponentMask combinedMask;
    fillBitmaskAndIndices<Types...>(bitmaskIndices, combinedMask, std::make_index_sequence<sizeof...(Types)>{});

    // will be filled with the component arrays of each archetype
    using PtrTuple = std::tuple<std::add_pointer_t<Types>...>;
    PtrTuple dataPointers;

    // run the func for all ements inside every archetype that has the required components
    for(auto& arch : archetypes)
    {
        if(arch.storageUsed == 0 || (arch.componentMask & combinedMask) != combinedMask)
            continue;

        fillPtrTuple<Types...>(bitmaskIndices, arch, dataPointers, std::make_index_sequence<sizeof...(Types)>{});

        std::apply(func, std::tuple_cat(dataPointers, std::make_tuple<uint32_t>(arch.storageUsed)));
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

template <typename C>
ECS::ComponentMask ECS::componentMaskFromComponentType()
{
    ComponentMask mask;
    mask.set(bitmaskIndexFromComponentType<C>());
    return mask;
}

template <typename C, typename... Args>
C* ECS::addComponent(Entity* entity, Args&&... args)
{
    auto archetypeEntryIt = entityLUT.find(entity->id);
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

    // move components into new archetype
    const uint32_t indexInOldArchetype = archEntry.inArrayIndex;
    const uint32_t indexInNewArchetype = newArchetype->storageUsed++;
    uint32_t currentBitmaskIndex = newMask.find_first();
    uint32_t oldArchComponentArrayIndex = 0;
    for(                                                    //
        uint32_t newArchComponentArrayIndex = 0;            //
        newArchComponentArrayIndex < newArchetypeCompCount; //
        newArchComponentArrayIndex++                        //
    )
    {
        const auto& componentInfo = componentInfos[currentBitmaskIndex];

        // Component existed in old archetype
        if(oldMask[currentBitmaskIndex])
        {
            std::byte* oldArchCompArray = (std::byte*)(oldArchetype->componentArrays[oldArchComponentArrayIndex]);
            std::byte* newArchCompArray = (std::byte*)(newArchetype->componentArrays[newArchComponentArrayIndex]);

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

            oldArchComponentArrayIndex++;
        }
        else
        {
            // Component did not exist in old archetype
            // so this must be the one being added
            C* newArchCompArray = (C*)(newArchetype->componentArrays[newArchComponentArrayIndex]);
            // this assumes the memory is "in destroyed state" (not sure about correct terminology)
            // -> construct instead of move into
            newComponent = new(&newArchCompArray[indexInNewArchetype]) C(std::forward<Args>(args)...);
        }
        currentBitmaskIndex = newMask.find_next(currentBitmaskIndex);
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

    // move components into new archetype
    const uint32_t indexInOldArchetype = archEntry.inArrayIndex;
    const uint32_t indexInNewArchetype = newArchetype->storageUsed++;
    uint32_t currentBitmaskIndex = oldMask.find_first();
    uint32_t newArchComponentArrayIndex = 0;
    for(                                                    //
        uint32_t oldArchComponentArrayIndex = 0;            //
        oldArchComponentArrayIndex < oldArchetypeCompCount; //
        oldArchComponentArrayIndex++                        //
    )
    {
        const auto& componentInfo = componentInfos[currentBitmaskIndex];

        std::byte* oldArchCompArray = (std::byte*)(oldArchetype->componentArrays[oldArchComponentArrayIndex]);

        const ComponentInfo::MoveConstrFunc_t moveFunc = componentInfo.moveFunc;
        const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;

        if(newMask[currentBitmaskIndex])
        {
            // Component still exists in new archetype
            std::byte* newArchCompArray = (std::byte*)(newArchetype->componentArrays[newArchComponentArrayIndex]);

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

            newArchComponentArrayIndex++;
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
        currentBitmaskIndex = oldMask.find_next(currentBitmaskIndex);
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
    {
        // todo: warn user, trying to retrieve component that entitiy doesnt hold
        return nullptr;
    }
    uint32_t arrayIndex = archetype.getArrayIndex(componentTypeBitmaskIndex);
    assert(entry.inArrayIndex < archetype.storageUsed);
    C* array = reinterpret_cast<C*>(archetype.componentArrays[arrayIndex]);
    return &(array[entry.inArrayIndex]);
}