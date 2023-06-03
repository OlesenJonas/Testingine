#include "ECS.hpp"
#include <cassert>

std::size_t ECS::ComponentMaskHash::operator()(const ComponentMask& key) const
{
    static_assert(std::is_same_v<ComponentMask::word_type, uint64_t>);
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

ECS::Entity::Entity(ECS& ecs, EntityID id) : ecs(ecs), id(id)
{
}

ECS::ECS()
{
    // create "Empty" (no components) archetype to hold empty entities by default
    archetypes.emplace_back(ComponentMask{}, *this);
    assert(archetypes[0].componentMask.none());
    archetypeLUT.emplace(std::make_pair(ComponentMask{}, 0));
}

ECS::Entity ECS::createEntity()
{
    Entity entity{*this, entityIDCounter++};

    Archetype& emptyArchetype = archetypes[0];
    uint32_t inArchetypeIndex = emptyArchetype.entityIDs.size();
    emptyArchetype.entityIDs.push_back(entity.id);

    auto insertion = entityLUT.emplace(std::make_pair(entity.id, ArchetypeEntry{0, inArchetypeIndex}));
    auto& entry = insertion.first->second;
    assert(archetypes[entry.archetypeIndex].entityIDs[inArchetypeIndex] == entity.id);

    return entity;
}

ECS::Entity ECS::getEntity(ECS::EntityID id)
{
    assert(entityLUT.find(id) != entityLUT.end());
    return Entity{*this, id};
}

ECS::EntityID ECS::Entity::getID() const
{
    return id;
}

ECS& ECS::Entity::getECS()
{
    return ecs;
}

uint32_t ECS::createArchetype(ComponentMask mask)
{
    assert(archetypeLUT.find(mask) == archetypeLUT.end());

    Archetype& newArchetype = archetypes.emplace_back(mask, *this);
    uint32_t newArchetypeIndex = archetypes.size() - 1;
    archetypeLUT.emplace(std::make_pair(mask, newArchetypeIndex));
    assert(archetypeLUT.find(mask) != archetypeLUT.end());

    return newArchetypeIndex;
}

ECS::Archetype::Archetype(ComponentMask mask, ECS& ecs) : componentMask(mask), ecs(ecs)
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

        uint32_t currentBitmaskIndex = componentMask.find_first();
        for(int i = 0; i < componentCount; i++)
        {
            const size_t componentSize = ecs.componentInfos[currentBitmaskIndex].size;
            componentArrays[i] = malloc(componentSize * storageCapacity);
            currentBitmaskIndex = componentMask.find_next(currentBitmaskIndex);
        }
    }
}

ECS::Archetype::Archetype(Archetype&& other) noexcept
    : componentMask(other.componentMask),
      ecs(other.ecs),
      componentArrays(std::move(other.componentArrays)),
      entityIDs(std::move(other.entityIDs)),
      storageUsed(other.storageUsed),
      storageCapacity(other.storageCapacity)
{
    assert(other.componentArrays.empty());
}

ECS::Archetype::~Archetype()
{
    uint32_t currentBitmaskIndex = componentMask.find_first();
    for(int i = 0; i < componentArrays.size(); i++)
    {
        ComponentInfo& componentInfo = ecs.componentInfos[currentBitmaskIndex];
        ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
        if(destroyFunc != nullptr)
        {
            for(int j = 0; j < storageUsed; j++)
            {
                std::byte* byteArray = (std::byte*)componentArrays[i];
                destroyFunc(&byteArray[j * componentInfo.size]);
            }
        }
        // destruct all objects that still exist in array
        free(componentArrays[i]);
        currentBitmaskIndex = componentMask.find_next(currentBitmaskIndex);
    }
    // dont have to worry about fixing any relations (LUT entries etc)
    // since this is only called
    //      a) on ECS shutdown
    //      b) after ecs.archetypes has been resized, in which case
    //         this should be in a moved-from (empty) state anyways
    //         where all links have already been updated outside
}

void ECS::Archetype::growStorage()
{
    size_t newCapacity = storageCapacity * 2;

    const uint32_t componentCount = componentMask.count();

    uint32_t currentBitmaskIndex = componentMask.find_first();
    for(int i = 0; i < componentCount; i++)
    {
        const auto& componentInfo = ecs.componentInfos[currentBitmaskIndex];

        std::byte* oldStorage = reinterpret_cast<std::byte*>(componentArrays[i]);
        std::byte* newStorage = (std::byte*)malloc(componentInfo.size * newCapacity);

        const ComponentInfo::MoveConstrFunc_t moveFunc = componentInfo.moveFunc;
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
        free(oldStorage);

        currentBitmaskIndex = componentMask.find_next(currentBitmaskIndex);
    }

    storageCapacity = newCapacity;
}

void ECS::Archetype::fixGap(uint32_t gapIndex)
{
    uint32_t oldEndIndex = entityIDs.size() - 1;
    if(!componentArrays.empty())
        storageUsed--;
    // move last group of components into gap to fill it
    uint32_t currentBitmaskIndex = componentMask.find_first();
    for(int i = 0; i < componentMask.count(); i++)
    {
        const auto& componentInfo = ecs.componentInfos[currentBitmaskIndex];

        std::byte* componentStorage = reinterpret_cast<std::byte*>(componentArrays[i]);

        const ComponentInfo::MoveConstrFunc_t moveFunc = componentInfo.moveFunc;
        const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
        if(moveFunc == nullptr)
        {
            if(gapIndex != oldEndIndex)
                // type is trivially relocatable, just memcpy components into new entry
                memcpy(
                    &componentStorage[gapIndex * componentInfo.size],
                    &componentStorage[oldEndIndex * componentInfo.size],
                    componentInfo.size);
        }
        else
        {
            // move from old end into gap, and destroy old end
            if(gapIndex != oldEndIndex)
                moveFunc(
                    &componentStorage[oldEndIndex * componentInfo.size],
                    &componentStorage[gapIndex * componentInfo.size]);
            destroyFunc(&componentStorage[oldEndIndex * componentInfo.size]);
        }

        currentBitmaskIndex = componentMask.find_next(currentBitmaskIndex);
    }

    // do same swap and delete on the entityID vector
    assert(entityIDs.size() - 1 == oldEndIndex);
    entityIDs[gapIndex] = entityIDs[oldEndIndex];
    entityIDs.pop_back();
    assert(storageCapacity == 0 || entityIDs.size() == storageUsed);

    // Also need to update entityLUT information of filler entity
    if(gapIndex != oldEndIndex)
    {
        auto iter = ecs.entityLUT.find(entityIDs[gapIndex]);
        assert(iter != ecs.entityLUT.end());
        assert(iter->second.inArrayIndex == oldEndIndex);
        iter->second.inArrayIndex = gapIndex;
    }
}

uint32_t ECS::Archetype::getArrayIndex(uint32_t bitmaskIndex)
{
    // shift away all bits (including bitmaskIndex one) and count how many remain, thats the index
    // into the component array array
    const int highBitsToEliminate = MAX_COMPONENT_TYPES - bitmaskIndex;
    ComponentMask mask = componentMask;
    mask <<= highBitsToEliminate;
    return mask.count();
}