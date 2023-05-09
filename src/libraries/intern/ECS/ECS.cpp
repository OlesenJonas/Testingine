#include "ECS.hpp"
#include "ECS/ECS.hpp"
#include "ECS/SharedTypes.hpp"

ECS::ECS()
{
    assert(ptr == nullptr);
    ptr = this;

    archetypes.emplace_back(ComponentMask{});
    assert(archetypes[0].componentMask.none());
    archetypeLUT.emplace(std::make_pair(ComponentMask{}, 0));
}

ECS::~ECS()
{
    ptr = nullptr;
}

ECS::Entity ECS::createEntity()
{
    Entity entity{};
    entity.id = entityIDCounter++;

    Archetype& emptyArchetype = archetypes[0];
    uint32_t inArchetypeIndex = emptyArchetype.entityIDs.size();
    emptyArchetype.entityIDs.push_back(entity.id);

    auto insertion = entityLUT.emplace(std::make_pair(entity.id, ArchetypeEntry{0, inArchetypeIndex}));
    auto& entry = insertion.first->second;
    assert(archetypes[entry.archetypeIndex].entityIDs[inArchetypeIndex] == entity.id);

    return entity;
}

EntityID ECS::Entity::getID() const
{
    return id;
}

uint32_t ECS::createArchetype(ComponentMask mask)
{
    assert(archetypeLUT.find(mask) == archetypeLUT.end());
    Archetype& newArchetype = archetypes.emplace_back(mask);
    uint32_t newArchetypeIndex = archetypes.size() - 1;
    archetypeLUT.emplace(std::make_pair(mask, newArchetypeIndex));
    assert(archetypeLUT.find(mask) != archetypeLUT.end());
    return newArchetypeIndex;
}

ECS::Archetype::Archetype(ComponentMask mask) : componentMask(mask)
{
    if(componentMask.none())
    {
        storageUsed = 0;
        storageCapacity = 0;
    }
    else
    {
        uint32_t componentCount = componentMask.count();
        assert(componentCount > 0);

        componentArrays.resize(componentCount);
        storageCapacity = 10;
        storageUsed = 0;

        uint32_t lastFind = componentMask.find_first();
        for(int i = 0; i < componentCount; i++)
        {
            const auto componentTypeID = lastFind;
            const size_t componentSize = ECS::get()->componentInfos[componentTypeID].size;
            componentArrays[i] = new std::byte[componentSize * storageCapacity];
            lastFind = componentMask.find_next(lastFind);
        }
    }
}

ECS::Archetype::Archetype(Archetype&& other) noexcept
    : componentMask(other.componentMask),
      componentArrays(std::move(other.componentArrays)),
      entityIDs(std::move(other.entityIDs)),
      storageUsed(other.storageUsed),
      storageCapacity(other.storageCapacity)
{
    assert(other.componentArrays.empty());
}

ECS::Archetype::~Archetype()
{
    for(int i = 0; i < componentArrays.size(); i++)
    {
        delete[]((std::byte*)componentArrays[i]);
    }
    // dont have to worry about fixing any relations (LUT entries etc)
    // since this is only called
    //      a) on ECS shutdown
    //      b) after ECS::archetypes has been resized, in which case
    //         this should be in a moved-from (empty) state anyways
}

void ECS::Archetype::growStorage()
{
    size_t newCapacity = storageCapacity * 2;

    const uint32_t componentCount = componentMask.count();

    uint32_t lastFind = componentMask.find_first();
    for(int i = 0; i < componentCount; i++)
    {
        const auto componentTypeID = lastFind;
        const auto& componentInfo = ECS::get()->componentInfos[componentTypeID];

        std::byte* oldStorage = reinterpret_cast<std::byte*>(componentArrays[i]);
        std::byte* newStorage = new std::byte[componentInfo.size * newCapacity];

        const ComponentInfo::MoveFunc_t moveFunc = componentInfo.moveFunc;
        const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
        if(moveFunc == nullptr)
        {
            // type is trivially relocatable, just memcpy components into new array
            memcpy(newStorage, oldStorage, componentInfo.size * storageCapacity);
        }
        else
        {
            // move all components over into new array
            for(int j = 0; j < storageCapacity; j++)
            {
                moveFunc(&oldStorage[j * componentInfo.size], &newStorage[j * componentInfo.size]);
            }
            // destroy whats left in the old array
            for(int j = 0; j < storageCapacity; j++)
            {
                destroyFunc(&oldStorage[j * componentInfo.size]);
            }
        }

        componentArrays[i] = newStorage;
        delete[] reinterpret_cast<std::byte*>(oldStorage);

        lastFind = componentMask.find_next(lastFind);
    }

    storageCapacity = newCapacity;
}

void ECS::Archetype::fixGap(uint32_t gapIndex)
{
    uint32_t oldEndIndex = storageUsed - 1;
    storageUsed--;
    // move last group of components into gap to fill it
    uint32_t lastFind = componentMask.find_first();
    for(int i = 0; i < componentMask.count(); i++)
    {
        const auto componentTypeID = lastFind;
        const auto& componentInfo = ECS::get()->componentInfos[componentTypeID];

        std::byte* componentStorage = reinterpret_cast<std::byte*>(componentArrays[i]);

        const ComponentInfo::MoveFunc_t moveFunc = componentInfo.moveFunc;
        const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
        if(moveFunc == nullptr)
        {
            if(gapIndex != oldEndIndex)
                // type is trivially relocatable, just memcpy components into new array
                memcpy(
                    &componentStorage[gapIndex * componentInfo.size],
                    &componentStorage[oldEndIndex * componentInfo.size],
                    componentInfo.size);
        }
        else
        {
            if(gapIndex != oldEndIndex)
                // move from old end into gap, and destroy old end
                moveFunc(
                    &componentStorage[oldEndIndex * componentInfo.size],
                    &componentStorage[gapIndex * componentInfo.size]);
            // destruction needs to happen anyways, even if gap is last element
            destroyFunc(&componentStorage[oldEndIndex * componentInfo.size]);
        }

        lastFind = componentMask.find_next(lastFind);
    }

    // do same swap and delete on the entityID vector
    assert(entityIDs.size() - 1 == oldEndIndex);
    entityIDs[gapIndex] = entityIDs[entityIDs.size() - 1];
    entityIDs.pop_back();
    assert(entityIDs.size() == storageUsed);

    // Also need to update entityLUT information of filler entity
    if(gapIndex != oldEndIndex)
    {
        auto iter = ECS::get()->entityLUT.find(entityIDs[gapIndex]);
        assert(iter != ECS::get()->entityLUT.end());
        assert(iter->second.inArrayIndex == oldEndIndex);
        iter->second.inArrayIndex = gapIndex;
    }
}