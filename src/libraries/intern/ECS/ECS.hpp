#pragma once

#include <Datastructures/ArrayHelpers.hpp>
#include <Datastructures/Concepts.hpp>
#include <EASTL/bitset.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "Helpers.hpp"

class ECSTester;

struct ECS
{
  private:
    inline static ECS* ptr = nullptr;

  public:
    [[nodiscard]] static inline ECS* impl()
    {
        assert(ptr != nullptr && "Static pointer for type ECS was not initialized!");
        return ptr;
    }

  private:
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
    Entity getEntity(EntityID id);

    template <typename C>
    void registerComponent();

    /*
        Execute something for all entities holding the requested components
        The given callable needs to have the signature:
            f(T1*, ..., TN*) when the template arguments of forEach are <T1,...,TN>
        - This function must not create, delete, add/remove components from existing entities!
            That could relocate internal storage which would break the internal iterators pointers!
    */
    template <typename... Types>
        requires(sizeof...(Types) >= 2) &&  //
                isDistinct<Types...>::value //
    void forEach(std::function<void(std::add_pointer_t<Types>...)> func);
    // Overload if only requesting a single component, since thats a lot easier
    template <typename Type>
    void forEach(std::function<void(std::add_pointer_t<Type>)> func);

  private:
    template <typename C>
    uint32_t bitmaskIndexFromComponentType();

    template <typename C, typename... Args>
    C* addComponent(EntityID entity, Args&&... args);

    template <typename C>
    void removeComponent(EntityID entity);

    template <typename C>
    C* getComponent(EntityID entity);

    /*
        returns index of new archetype inside archetypes array
    */
    uint32_t createArchetype(ComponentMask mask);

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
        explicit Entity(EntityID id);

        EntityID id;

        friend ECS;
    };

  private:
    struct ComponentMaskHash
    {
        std::size_t operator()(const ComponentMask& key) const;
    };

    struct Archetype
    {
        explicit Archetype(ComponentMask mask);
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

        void growStorage();
        /*
            removes entry and fixes gap in storage array by swapping with end
            assumes memory is not yet in a "destroyed state" (may be moved from though)
        */
        void removeEntry(uint32_t index);
        uint32_t getArrayIndex(uint32_t bitmaskIndex);
    };
    static_assert(std::is_move_constructible<Archetype>::value);

    /*
        Represents where an entity is stored
            archetypeIndex is the index into the ECS' "archetypes" array
            inArrayIndex is the index of the entity inside that archetype's component arrays
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
        MoveConstrFunc_t moveConstrFunc = nullptr;
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
    uint32_t freeComponentBitmaskIndex = 0;
    std::unordered_map<ECSHelpers::TypeKey, uint32_t> componentTypeKeyToBitmaskIndexLUT;
    /*
        Data arrays
    */
    std::array<ComponentInfo, MAX_COMPONENT_TYPES> componentInfos{};
    std::vector<Archetype> archetypes;

    friend ECSTester;
};

#include "Entity.tpp"

#include "ECS.tpp"