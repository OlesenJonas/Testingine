#pragma once

#include <Datastructures/ArrayHelpers.hpp>
#include <Datastructures/Concepts.hpp>
#include <EASTL/bitset.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Helpers.hpp"

class ECSTester;

struct ECS
{
  private:
    struct ComponentInfo;
    struct Archetype;
    struct ArchetypeEntry;

  public:
    struct Entity;
    using EntityID = uint32_t;
    // just here to prevent copy paste errors
    static_assert(std::is_unsigned_v<EntityID>);
    constexpr static uint32_t MAX_COMPONENT_TYPES = 32;
    // not sure if making this a bitmask is actually a net win, some things are simpler, some more complicated
    using ComponentMask = eastl::bitset<MAX_COMPONENT_TYPES, uint64_t>;

    ECS();

    Entity createEntity();

    template <typename C>
    void registerComponent();

    /*
        Execute something for all entities holding the requested components
        The given callable needs to have the signature:
            f(T1*, ..., TN*, uint32_t count) when the template arguments of forEach are <T1,...,TN>
        - This function can be called multiple times internally! (once for each archetype containing all requested
            components). For that reason "count" may not necessarily be the total amount of entities with those
            components.
        - This function must not create, delete, change existing entities!
            That could relocate internal storage which would break the given pointers!
    */
    template <typename... Types, typename Func>
        requires(sizeof...(Types) >= 2) &&                                   //
                isDistinct<Types...>::value &&                               //
                std::invocable<Func, std::add_pointer_t<Types>..., uint32_t> //
    void forEach(Func func);
    // Overload if only requesting a single component, since thats a lot easier
    template <typename Type, typename Func>
        requires std::invocable<Func, std::add_pointer_t<Type>, uint32_t> //
    void forEach(Func func);

  private:
    template <typename C>
    uint32_t bitmaskIndexFromComponentType();

    template <typename C>
    ComponentMask componentMaskFromComponentType();

    template <typename C, typename... Args>
    C* addComponent(Entity* entity, Args&&... args);

    template <typename C>
    void removeComponent(Entity* entity);

    template <typename C>
    C* getComponent(Entity* entity);

    uint32_t createArchetype(ComponentMask mask);

    template <typename... Types, std::size_t... I>
    void fillBitmaskAndIndices(
        ArrayReference<uint32_t, sizeof...(Types)> indices, ComponentMask& mask, std::index_sequence<I...>);

    template <typename... Types, std::size_t... I>
    void fillPtrTuple(
        ArrayReference<uint32_t, sizeof...(Types)> indices,
        ECS::Archetype& archetype,
        std::tuple<std::add_pointer_t<Types>...>& tuple,
        std::index_sequence<I...>);

    //------------------------ Struct Definitions
  public:
    struct Entity
    {
        EntityID getID() const;

        template <typename C, typename... Args>
            requires std::constructible_from<C, Args...>
        C* addComponent(Args&&... args);

        template <typename C>
        void removeComponent();

        template <typename C>
        C* getComponent();

      private:
        // only allow construction by ECS
        explicit Entity(ECS& ecs, EntityID id);

        EntityID id;
        // not sure if its worth it to store the reference here just for the nicer syntax of
        //   entity.addComponent
        //   instead of
        //   ecs.addComponent(entity, params)
        //   will see
        ECS& ecs;

        friend ECS;
    };

  private:
    struct ComponentMaskHash
    {
        std::size_t operator()(const ComponentMask& key) const;
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
        uint32_t getArrayIndex(uint32_t bitmaskIndex);
    };
    static_assert(std::is_move_constructible<Archetype>::value);

    /*
        Represents where an entity is stored
            archetypeIndex is the index into the ECS' "archetypes" array
        and inArrayIndex is the index of the entity inside that archetype's component arrays
    */
    struct ArchetypeEntry
    {
        uint32_t archetypeIndex = 0xFFFFFFFF;
        uint32_t inArrayIndex = 0xFFFFFFFF;
    };

    struct ComponentInfo
    {
        // clang-format off
        /*
          Function that constructs a component of this type by moving another instance of this into it
          - This constructs because it assumes that the destination memory is uninitialized, which is true in most cases.
            Though in cases where a component in the middle of an array is deleted, a move "replace" (swapping with last and
            decreasing size) to fix the gap would be enough.
            In order to not need to differentiate though, everytime a component is removed the object is also fully destructed.
            So essentially there may be cases where a destructor + constructor is called too much but it works for now and means
            just a single function is needed here, instead of MoveConstruct() *and* Move()
        */
        // clang-format on
        using MoveConstrFunc_t = void (*)(void* srcObject, void* dstptr);
        using DestroyFunc_t = void (*)(void*);
        size_t size = 0;
        MoveConstrFunc_t moveFunc = nullptr;
        DestroyFunc_t destroyFunc = nullptr;
    };

    //------------------------ Private Members

    /*
        Growing counter for generating "unique" ids for entities
        Very bare bones, doesnt hold under re-loading, serialization etc
    */
    EntityID entityIDCounter = 0;
    /*
        Get the index of an archetype given a bitmask of components it should hold
    */
    std::unordered_map<ComponentMask, uint32_t, ComponentMaskHash, std::equal_to<>> archetypeLUT;
    /*
        Retrieve information about its storage from an entities' id
    */
    std::unordered_map<EntityID, ArchetypeEntry> entityLUT;
    /*
        LUT to map a bitmask index to each unique type
    */
    uint32_t bitmaskIndexCounter = 0;
    std::unordered_map<ECSHelpers::TypeKey, uint32_t> componentTypeKeyToBitmaskIndexLUT;
    /*
        Data arrays
    */
    std::vector<ComponentInfo> componentInfos;
    std::vector<Archetype> archetypes;

    friend ECSTester;
};

#include "Entity.tpp"

#include "ECS.tpp"