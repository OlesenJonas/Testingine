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

ECS::Entity::Entity(EntityID id) : id(id) {}

ECS::ECS()
{
    ptr = this;
    // create "Empty" (no components) archetype to hold empty entities by default
    archetypes.emplace_back(ComponentMask{});
    assert(archetypes[0].componentMask.none());
    archetypeLUT.emplace(std::make_pair(ComponentMask{}, 0));
}

ECS::Entity ECS::createEntity()
{
    Entity entity{entityIDCounter++};

    Archetype& emptyArchetype = archetypes[0];
    uint32_t inArchetypeIndex = emptyArchetype.entityIDs.size();
    emptyArchetype.entityIDs.push_back(entity.id);
    emptyArchetype.storageUsed++;

    auto insertion = entityLUT.emplace(std::make_pair(entity.id, ArchetypeEntry{0, inArchetypeIndex}));
    auto& entry = insertion.first->second;
    assert(archetypes[entry.archetypeIndex].entityIDs[inArchetypeIndex] == entity.id);

    return entity;
}

ECS::Entity ECS::getEntity(ECS::EntityID id)
{
    assert(entityLUT.find(id) != entityLUT.end());
    return Entity{id};
}

ECS::EntityID ECS::Entity::getID() const { return id; }

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
        storageCapacity = ~size_t(0);
        assert(componentArrays.empty());
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
            const size_t componentSize = ECS::impl()->componentInfos[currentBitmaskIndex].size;
            componentArrays[i] = malloc(componentSize * storageCapacity);
            currentBitmaskIndex = componentMask.find_next(currentBitmaskIndex);
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
    uint32_t currentBitmaskIndex = componentMask.find_first();
    for(int i = 0; i < componentArrays.size(); i++)
    {
        ComponentInfo& componentInfo = ECS::impl()->componentInfos[currentBitmaskIndex];
        ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
        if(destroyFunc != nullptr)
        {
            for(int j = 0; j < storageUsed; j++)
            {
                auto* componentArray = (std::byte*)componentArrays[i];
                destroyFunc(&componentArray[j * componentInfo.size]);
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
        const auto& componentInfo = ECS::impl()->componentInfos[currentBitmaskIndex];

        auto* oldStorage = reinterpret_cast<std::byte*>(componentArrays[i]);
        auto* newStorage = (std::byte*)malloc(componentInfo.size * newCapacity);

        const ComponentInfo::MoveConstrFunc_t moveFunc = componentInfo.moveConstrFunc;
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

void ECS::Archetype::removeEntry(uint32_t index)
{
    assert(storageUsed > 0);
    uint32_t oldEndIndex = entityIDs.size() - 1;

    uint32_t currentBitmaskIndex = componentMask.find_first();
    for(int i = 0; i < componentMask.count(); i++)
    {
        const auto& componentInfo = ECS::impl()->componentInfos[currentBitmaskIndex];

        auto* componentStorage = reinterpret_cast<std::byte*>(componentArrays[i]);

        const ComponentInfo::MoveConstrFunc_t moveConstrFunc = componentInfo.moveConstrFunc;
        const ComponentInfo::DestroyFunc_t destroyFunc = componentInfo.destroyFunc;
        if(moveConstrFunc == nullptr)
        {
            // type is trivially relocatable, just memcpy component into gap slot
            memcpy(
                &componentStorage[index * componentInfo.size],
                &componentStorage[oldEndIndex * componentInfo.size],
                componentInfo.size);
        }
        else
        {
            destroyFunc(&componentStorage[index * componentInfo.size]);
            moveConstrFunc(
                &componentStorage[oldEndIndex * componentInfo.size],
                &componentStorage[index * componentInfo.size]);
            destroyFunc(&componentStorage[oldEndIndex * componentInfo.size]);
        }

        currentBitmaskIndex = componentMask.find_next(currentBitmaskIndex);
    }

    // do same swap and delete on the entityID vector
    assert(entityIDs.size() - 1 == oldEndIndex);
    entityIDs[index] = entityIDs[oldEndIndex];
    entityIDs.pop_back();
    storageUsed--;
    assert(entityIDs.size() == storageUsed);

    // Also need to update entityLUT information of filler entity
    if(index != oldEndIndex)
    {
        auto iter = ECS::impl()->entityLUT.find(entityIDs[index]);
        assert(iter != ECS::impl()->entityLUT.end());
        assert(iter->second.inArrayIndex == oldEndIndex);
        iter->second.inArrayIndex = index;
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