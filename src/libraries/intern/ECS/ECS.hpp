#pragma once

#include <cstdint>
#include <intern/Misc/Macros.hpp>
#include <type_traits>
#include <unordered_map>

#include "SharedTypes.hpp"

struct ECS
{
    CREATE_STATIC_GETTER(ECS);

  private:
    struct Entity;
    struct ComponentTypeIDGenerator;
    template <typename C>
    struct ComponentTypeIDCache;
    struct ComponentInfo;
    struct Archetype;
    struct ArchetypeEntry;

  public:
    Entity createEntity();

    template <typename C>
    void registerComponent();

  private:
    template <typename C, typename... Args>
    C& addComponent(Entity* entity, Args&&... args);

    template <typename C>
    void removeComponent(Entity* entity);

    template <typename C>
    C* getComponent(Entity* entity);

    uint32_t createArchetype(ComponentMask mask);

    //------------------------ Private Members

    EntityID entityIDCounter = 0;
    std::vector<ComponentInfo> componentInfos;
    std::vector<Archetype> archetypes;
    std::unordered_map<ComponentMask, uint32_t, ComponentMaskHash> archetypeLUT;
    std::unordered_map<EntityID, ArchetypeEntry> entityLUT;

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
        C& getComponent();

      private:
        // only allow construction by ECS
        Entity() = default;

        EntityID id;

        friend ECS;
    };

    struct Archetype
    {
        explicit Archetype(ComponentMask mask);
        // this is technically stored twice, since its also the key
        // used to store the Archetype lookup entry
        const ComponentMask componentMask;
        std::vector<void*> componentArrays;
        std::vector<EntityID> entityIDs;
        size_t storageUsed;
        size_t storageCapacity;

        void growStorage();
    };
    static_assert(std::is_move_constructible<Archetype>::value);

    struct ArchetypeEntry
    {
        uint32_t archetypeIndex = 0xFFFFFFFF;
        uint32_t inArrayIndex = 0xFFFFFFFF;
    };

    template <typename C>
    struct ComponentTypeIDCache
    {
        static ComponentTypeID getID();

      private:
        inline static ComponentTypeID id = ~ComponentTypeID(0);
    };

    struct ComponentTypeIDGenerator
    {
      public:
        template <typename C>
        static const IDType GetNewID();

      private:
        inline static IDType counter = 0;
    };

    struct ComponentInfo
    {
        using MoveFunc_t = void (*)(void* src, void* dst);
        using DestroyFunc_t = void (*)(void*);
        size_t size = 0;
        MoveFunc_t moveFunc = nullptr;
        DestroyFunc_t destroyFunc = nullptr;
    };
};

#include "Entity.tpp"

#include "ECS.tpp"
