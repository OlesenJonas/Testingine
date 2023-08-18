#pragma once

#include "../DynamicBitset.hpp"
#include "Handle.hpp"
#include "PoolHelpers.hpp"

#include <cassert>
#include <functional>
#include <type_traits>

// Handle and Pool types as shown in https://twitter.com/SebAaltonen/status/1562747716584648704 and realted tweets
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

template <uint32_t limit, typename T>
// TODO: not sure which concepts should be used here...
//   internally memory is just memcopy-ed on resize, so the object should just be movable, but they could have
//   destructors.
//   std::is_trivially_relocatable sounds perfect but doesnt exist yet...
class PoolImpl
{
  public:
    PoolImpl() = default;

    explicit PoolImpl(uint32_t initialCapacity) { init(initialCapacity); }

    bool init(uint32_t initialCapacity)
    {
        capacity = std::min(limit, initialCapacity);
        freeArray = DynamicBitset{capacity};
        storage = static_cast<T*>(POOL_ALLOC(capacity * sizeof(T), alignof(T))); // NOLINT
        freeArray.fill();
        generations = new uint32_t[capacity]; // NOLINT
        memset(generations, 0u, 4 * capacity);
        // otherwise first insert in slot0 will have handle {0,0} which is representation of invalid
        generations[0] = 1;

        return true;
    }
    void shutdown()
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

        capacity = 0;
        storage = nullptr;
        freeArray.resize(0);
        generations = nullptr;
    }
    ~PoolImpl() { shutdown(); }

    template <class... ArgTypes>
    Handle<T> insert(ArgTypes&&... args)
        requires std::is_constructible_v<T, ArgTypes...>
    {
        if(!freeArray.anyBitSet())
        {
            if constexpr(isLimited)
            {
                if(capacity == limit)
                    return Handle<T>::Null();
            }
            grow();
        }

        uint32_t index = freeArray.getFirstBitSet();
        T* newT = new(&storage[index]) T(std::forward<ArgTypes>(args)...); // placement new
        freeArray.clearBit(index);

        const Handle<T> newHandle{index, generations[index]};
        assert(isHandleValid(newHandle));

        return {index, generations[index]};
    }

    void remove(Handle<T> handle)
    {
        if(!isHandleValid(handle))
        {
            // todo: return boolean indicating nothing happened?
            return;
        }
        generations[handle.getIndex()]++;
        storage[handle.getIndex()].~T();
        freeArray.setBit(handle.getIndex());
    }

    inline T* get(Handle<T> handle)
    {
        if(!isHandleValid(handle))
            return nullptr;
        return &storage[handle.getIndex()];
    }

    Handle<T> getFirst()
    {
        if(!freeArray.anyBitClear())
            return Handle<T>::Null();

        uint32_t index = freeArray.getFirstBitClear();

        return {index, generations[index]};
    }

    bool isHandleValid(Handle<T> handle)
    {
        assert(handle.getIndex() < capacity);

        return handle.getGeneration() == generations[handle.getIndex()];
    }

    Handle<T> find(std::function<bool(T*)> pred)
    {
        for(uint32_t i = 0; i < capacity; i++)
        {
            if(!freeArray.getBit(i) && pred(&storage[i]))
            {
                return Handle<T>{i, generations[i]};
            }
        }
        return Handle<T>::Null();
    }

  private:
    void grow()
    {
        uint32_t oldCapacity = capacity;
        T* oldStorage = storage;
        uint32_t* oldGenerations = generations;

        capacity = std::min(limit, capacity * 2); // growing factor

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
                oldStorage[i].~T();
            }
        }

        POOL_FREE(oldStorage);
        delete[] oldGenerations;
    }

    // if T is trivially_relocatable then we can grow the Pool with simple memmoves
    static constexpr bool canUseMemcpy = PoolHelper::is_trivially_relocatable<T>;
    static constexpr bool isLimited = limit != PoolHelper::unlimited;

    uint32_t capacity = 0;
    T* storage = nullptr;
    DynamicBitset freeArray{0};
    uint32_t* generations = nullptr;
};

template <typename T>
using Pool = PoolImpl<PoolHelper::unlimited, T>;

template <uint32_t limit, typename T>
using PoolLimited = PoolImpl<limit, T>;