#include "ECS.hpp"

ECS::Entity ECS::createEntity()
{
    Entity entity{};
    entity.id = entityIDCounter++;

    return entity;
}

EntityID ECS::Entity::getID() const
{
    return id;
}

uint32_t ECS::createArchetype(ComponentMask mask)
{
    Archetype& newArchetype = archetypes.emplace_back(mask);
    uint32_t newArchetypeIndex = archetypes.size() - 1;
    archetypeLUT.try_emplace(mask, newArchetypeIndex);
    return newArchetypeIndex;
}

ECS::Archetype::Archetype(ComponentMask mask) : componentMask(mask)
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