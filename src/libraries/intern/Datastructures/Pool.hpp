#pragma once

#include "DynamicBitset.hpp"

#include <Engine/Misc/Concepts.hpp>
#include <cassert>
#include <type_traits>

/*
    not sure this is correct yet, will see
        https://stackoverflow.com/questions/222557/what-uses-are-there-for-placement-new
        https://en.cppreference.com/w/cpp/language/new
        https://www.stroustrup.com/bs_faq2.html#placement-delete
        https://stackoverflow.com/questions/11781724/do-i-really-have-to-worry-about-alignment-when-using-placement-new-operator
*/

#ifdef _WIN32
    #include <malloc.h>
    #define POOL_ALLOC(size, alignment) _aligned_malloc(size, alignment)
    #define POOL_FREE _aligned_free
#else
    #include <cstdlib>
    #define POOL_ALLOC(size, alignment) std::aligned_alloc(alignment, size)
    #define POOL_FREE std::free
#endif

// Handle and Pool types as shown in https://twitter.com/SebAaltonen/status/1562747716584648704 and realted tweets
template <typename T>
class Handle
{
  public:
    Handle() = default;
    Handle(uint32_t index, uint32_t generation) : index(index), generation(generation){};
    static Handle Invalid()
    {
        return {0, 0};
    }
    [[nodiscard]] bool isValid() const
    {
        return generation != 0u;
    }
    bool operator==(const Handle<T>& other) const
    {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(const Handle<T>& other) const
    {
        return index != other.index || generation != other.generation;
    }
    [[nodiscard]] uint32_t hash() const
    {
        return (uint32_t(index) << 16u) + uint32_t(generation);
    }

  private:
    uint16_t index = 0;
    uint16_t generation = 0;

    template <typename U>
    friend class Pool;
    friend class ResourceManager;
};

template <typename T>
// TODO: not sure which concepts should be used here...
//   internally memory is just memcopy-ed on resize, so the object should just be movable, but they could have
//   destructors.
//   std::is_trivially_relocatable sounds perfect but doesnt exist yet...
//   For now I could derive from an empty class that acts as a kind of attribute and then check for is_derived I
//   think
class Pool
{
  public:
    explicit Pool(size_t initialCapacity) : capacity(initialCapacity), freeArray(initialCapacity)
    {
        storage = static_cast<T*>(POOL_ALLOC(initialCapacity * sizeof(T), alignof(T))); // NOLINT
        freeArray.fill();
        generations = new uint32_t[initialCapacity]; // NOLINT
        memset(generations, 0u, 4 * initialCapacity);
    }
    ~Pool()
    {
        for(auto i = 0; i < capacity; i++)
        {
            if(!freeArray.getBit(i))
            {
                storage[i].~T();
            }
        }
        POOL_FREE(storage);
        delete[] generations;
    }

    template <class... ArgTypes>
    Handle<T> insert(ArgTypes&&... args)
    {
        if(!freeArray.anyBitSet())
            grow();

        uint32_t index = freeArray.getFirstBitSet();
        generations[index]++;
        T* newT = new(&storage[index]) T(std::forward<ArgTypes>(args)...); // placement new
        freeArray.clearBit(index);

        return {index, generations[index]};
    }

    void remove(Handle<T> handle)
    {
        if(!isHandleValid(handle))
        {
            // todo: return boolean indicating nothing happened?
            return;
        }
        storage[handle.index].~T();
        freeArray.setBit(handle.index);
    }

    inline T* get(Handle<T> handle)
    {
        if(!isHandleValid(handle))
            return nullptr;
        return &storage[handle.index];
    }

    Handle<T> getFirst()
    {
        if(!freeArray.anyBitClear())
            return {0, 0};

        uint32_t index = freeArray.getFirstBitClear();

        return {index, generations[index]};
    }

    bool isHandleValid(Handle<T> handle)
    {
        assert(handle.index < capacity);

        return generations[handle.index] == handle.generation && !freeArray.getBit(handle.index);
    }

  private:
    void grow()
    {
        size_t oldCapacity = capacity;
        T* oldStorage = storage;
        uint32_t* oldGenerations = generations;

        capacity *= 2; // growing factor

        storage = static_cast<T*>(POOL_ALLOC(capacity * sizeof(T), alignof(T))); // NOLINT
        generations = new uint32_t[capacity];                                    // NOLINT
        // todo: really just need to memset the part thats not memcpy-ed into afterwards
        memset(generations, 0u, 4 * capacity);
        freeArray.resize(capacity);

        memcpy(generations, oldGenerations, oldCapacity * sizeof(uint32_t));
        freeArray.fill(oldCapacity, capacity - 1);

        if constexpr(canUseMemcpy)
        {
            memcpy(storage, oldStorage, oldCapacity * sizeof(T));
        }
        else
        {
            for(int i = 0; i < oldCapacity; i++)
            {
                // move construct from old storage into new
                // C++20 has construct_at, just trying it out
                std::construct_at(&storage[i], std::move(oldStorage[i]));
                // explicitly destroy left over object in old storage
                storage[i].~T();
            }
        }

        POOL_FREE(oldStorage);
        delete[] oldGenerations;
    }

    // if T is trivially_relocatable then we can grow the Pool with simple memmoves
    static constexpr bool canUseMemcpy = is_trivially_relocatable<T>;

    size_t capacity = 0;
    T* storage;
    DynamicBitset freeArray;
    uint32_t* generations;
};