#pragma once

#include "DynamicBitset.hpp"

#include <cassert>
#include <type_traits>
#include <vcruntime_string.h>

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

// as shown in https://twitter.com/SebAaltonen/status/1562747716584648704 and realted tweets
template <typename T>
class Handle
{
  public:
    Handle() = default;
    Handle(uint32_t index, uint32_t generation) : index(index), generation(generation){};
    [[nodiscard]] bool isValid() const
    {
        return generation != 0u;
    }

  private:
    uint32_t index = 0;
    uint32_t generation = 0;

    template <typename U>
    friend class Pool;
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

    T* get(Handle<T> handle)
    {
        if(!isHandleValid(handle))
            return nullptr;
        return storage[handle.index];
    }

  private:
    bool isHandleValid(Handle<T> handle)
    {
        assert(handle.index < capacity);

        return generations[handle.index] == handle.generation && !freeArray.getBit(handle.index);
    }

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

        memcpy(storage, oldStorage, oldCapacity * sizeof(T));
        memcpy(generations, oldGenerations, oldCapacity * sizeof(uint32_t));
        freeArray.fill(oldCapacity, capacity - 1);

        POOL_FREE(oldStorage);
        delete[] oldGenerations;
    }

    size_t capacity = 0;
    T* storage;
    DynamicBitset freeArray;
    uint32_t* generations;
};