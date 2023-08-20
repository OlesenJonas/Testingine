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
        inUseMask = DynamicBitset{capacity};
        inUseMask.clear();
        storage = static_cast<T*>(POOL_ALLOC(capacity * sizeof(T), alignof(T))); // NOLINT
        generations = new uint32_t[capacity];                                    // NOLINT
        memset(generations, 0u, 4 * capacity);
        // otherwise first insert in slot0 will have handle {0,0} which is representation of invalid
        generations[0] = 1;

        return true;
    }
    void shutdown()
    {
        for(auto i = 0; i < capacity; i++)
        {
            if(inUseMask.getBit(i))
            {
                storage[i].~T();
            }
        }
        POOL_FREE(storage);
        delete[] generations;

        capacity = 0;
        storage = nullptr;
        inUseMask.resize(0);
        generations = nullptr;
    }
    ~PoolImpl() { shutdown(); }

    template <class... ArgTypes>
    Handle<T> insert(ArgTypes&&... args)
        requires std::is_constructible_v<T, ArgTypes...>
    {
        if(!inUseMask.anyBitClear())
        {
            if constexpr(isLimited)
            {
                if(capacity == limit)
                    return Handle<T>::Null();
            }
            grow();
        }

        uint32_t index = inUseMask.getFirstBitClear();
        T* newT = new(&storage[index]) T(std::forward<ArgTypes>(args)...); // placement new
        inUseMask.setBit(index);

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
        inUseMask.clearBit(handle.getIndex());
    }

    inline T* get(Handle<T> handle)
    {
        if(!isHandleValid(handle))
            return nullptr;
        return &storage[handle.getIndex()];
    }

    Handle<T> getFirst()
    {
        if(!inUseMask.anyBitSet())
            return Handle<T>::Null();

        uint32_t index = inUseMask.getFirstBitSet();

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
            if(inUseMask.getBit(i) && pred(&storage[i]))
            {
                return Handle<T>{i, generations[i]};
            }
        }
        return Handle<T>::Null();
    }

    // Basic pointer to enable iterating over elements
    // iterate directly over the elements inside the storage array
    //   TODO: could have another iterator that uses Handles which would be a bit more robust in some cases I think
    template <bool isConst = true>
    struct DirectIterator
    {
        using iterator_type = std::forward_iterator_tag;
        using difference_type = uint32_t;

        using Tptr = std::conditional<isConst, const T*, T*>::type;
        using Tref = std::conditional<isConst, const T&, T&>::type;

        using Pool = std::conditional<isConst, const PoolImpl<limit, T>, PoolImpl<limit, T>>::type;

        DirectIterator(uint32_t start, Pool* owner) : pool(owner), index(start) {}

        // would auto here automatically make the return type either T* or const T* ?
        Tptr operator*() const { return &pool->storage[index]; };
        Tref operator->() const { return pool->storage[index]; };
        // TODO: handle const correctness for handles
        Handle<T> asHandle() { return {index, pool->generations[index]}; }

        DirectIterator& operator++()
        {
            // TODO: test if its faster to just for loop over all bits and get them here
            //       maybe with caching the current word
            // TODO: implement alternative iterator that caches full words of the freeLists bitmask?
            //       would be invalidated by insertions and removals (since cached mask != actual free mask), but
            //       faster
            index = pool->inUseMask.getNextBitSet(index);
            return *this;
        }

        DirectIterator operator++(int)
        {
            DirectIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const DirectIterator& a, const DirectIterator& b)
        {
            return a.pool == b.pool && a.index == b.index;
        }
        friend bool operator!=(const DirectIterator& a, const DirectIterator& b)
        {
            return a.pool != b.pool || a.index != b.index;
        }

      private:
        uint32_t index;
        Pool* pool;
    };

    DirectIterator<true> cbegin() const { return {inUseMask.getFirstBitSet(), this}; }
    DirectIterator<true> begin() const { return cbegin(); }
    DirectIterator<false> begin() { return {inUseMask.getFirstBitSet(), this}; }
    DirectIterator<true> cend() const { return {0xFFFFFFFF, this}; }
    DirectIterator<true> end() const { return cend(); }
    DirectIterator<false> end() { return {0xFFFFFFFF, this}; }

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
        inUseMask.resize(capacity);

        memcpy(generations, oldGenerations, oldCapacity * sizeof(uint32_t));
        inUseMask.clear(oldCapacity, capacity - 1);

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
    // bit set == element currently contains active object
    DynamicBitset inUseMask{0};
    uint32_t* generations = nullptr;
};

template <typename T>
using Pool = PoolImpl<PoolHelper::unlimited, T>;

template <uint32_t limit, typename T>
using PoolLimited = PoolImpl<limit, T>;