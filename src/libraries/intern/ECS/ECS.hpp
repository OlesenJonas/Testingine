#pragma once

#include <bitset>
#include <cstdint>
#include <intern/Misc/Macros.hpp>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Helpers.hpp"
#include "SharedTypes.hpp"

#ifdef TESTER_CLASS
class TESTER_CLASS;
#endif

struct ECS
{
  private:
    struct Entity;
    struct ComponentInfo;
    struct Archetype;
    struct ArchetypeEntry;

  public:
    ECS();

    Entity createEntity();

    template <typename C>
    void registerComponent();

  private:
    template <typename C>
    uint32_t bitmaskIndexFromComponentType();

    template <typename C, typename... Args>
    C* addComponent(Entity* entity, Args&&... args);

    template <typename C>
    void removeComponent(Entity* entity);

    template <typename C>
    C* getComponent(Entity* entity);

    uint32_t createArchetype(ComponentMask mask);

    //------------------------ Struct Definitions

    struct Entity
    {
        EntityID getID() const;

        template <typename C, typename... Args>
            requires std::constructible_from<C, Args...>
        void addComponent(Args&&... args);

        template <typename C>
        void removeComponent();

        template <typename C>
        C* getComponent();

      private:
        // only allow construction by ECS
        Entity(ECS& ecs);

        EntityID id;
        // not sure if its worth it to store the reference here just for the nicer syntax of
        //   entity.addComponent
        //   instead of
        //   ecs.addComponent(entity, params)
        //   will see
        ECS& ecs;

        friend ECS;
    };

    struct Archetype
    {
        explicit Archetype(ComponentMask mask, ECS& ecs);
        Archetype(Archetype&& other) noexcept;
        ~Archetype();
        Archetype(const Archetype&) = delete;
        // this is technically stored twice, since its also the key
        // used to store the Archetype lookup entry
        const ComponentMask componentMask;
        std::vector<void*> componentArrays;
        std::vector<EntityID> entityIDs;
        size_t storageUsed;
        size_t storageCapacity;
        ECS& ecs;

        void growStorage();
        void fixGap(uint32_t gapIndex);
    };
    static_assert(std::is_move_constructible<Archetype>::value);

    struct ArchetypeEntry
    {
        uint32_t archetypeIndex = 0xFFFFFFFF;
        uint32_t inArrayIndex = 0xFFFFFFFF;
    };

    struct ComponentInfo
    {
        using MoveFunc_t = void (*)(void* src, void* dst);
        using DestroyFunc_t = void (*)(void*);
        size_t size = 0;
        MoveFunc_t moveFunc = nullptr;
        DestroyFunc_t destroyFunc = nullptr;
    };

    //------------------------ Private Members

    EntityID entityIDCounter = 0;
    std::vector<ComponentInfo> componentInfos;
    std::vector<Archetype> archetypes;
    std::unordered_map<ComponentMask, uint32_t> archetypeLUT;
    std::unordered_map<EntityID, ArchetypeEntry> entityLUT;
    uint32_t bitmaskIndexCounter = 0;
    std::unordered_map<ECSHelpers::TypeKey, uint32_t> componentTypeKeyToBitmaskIndexLUT;

#ifdef TESTER_CLASS
    friend TESTER_CLASS;
#endif
};

#include "Entity.tpp"

#include "ECS.tpp"
