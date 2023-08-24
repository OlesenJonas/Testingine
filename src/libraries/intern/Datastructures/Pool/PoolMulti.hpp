#pragma once

#include "../DynamicBitset.hpp"
#include "Handle.hpp"
#include "PoolHelpers.hpp"

#include <cassert>
#include <functional>
#include <type_traits>

// Extension of the basic "Pool" structure that can hold multiple different structs internally in an SoA layout

#ifdef _WIN32
    #include <malloc.h>
    #define POOL_ALLOC(size, alignment) _aligned_malloc(size, alignment)
    #define POOL_FREE _aligned_free
#else
    #include <cstdlib>
    #define POOL_ALLOC(size, alignment) std::aligned_alloc(alignment, size)
    #define POOL_FREE std::free
#endif

template <uint32_t limit, typename... Ts>
class MultiPoolImpl
{
  public:
    MultiPoolImpl() = default;

    explicit MultiPoolImpl(uint32_t initialCapacity) { init(initialCapacity); }

    bool init(uint32_t initialCapacity)
    {
        capacity = std::min(limit, initialCapacity);
        inUseMask = DynamicBitset{capacity};
        inUseMask.clear();

        storage = new void*[sizeof...(Ts)];

        // (Ab-)using fold expression for loop over types
        int i = 0;
        (
            [&]()
            {
                storage[i] = static_cast<Ts*>(POOL_ALLOC(capacity * sizeof(Ts), alignof(Ts))); // NOLINT
                i++;
            }(),
            ... //
        );

        generations = new uint32_t[capacity]; // NOLINT
        memset(generations, 0u, 4 * capacity);
        // otherwise first insert in slot0 will have handle {0,0} which is representation of invalid
        generations[0] = 1;

        return true;
    }
    void shutdown()
    {
        if(storage == nullptr)
            return; // was already shutdown

        int type = 0;
        (
            [&]()
            {
                Ts* t_storage = static_cast<Ts*>(storage[type]);
                for(auto i = 0; i < capacity; i++)
                {
                    if(inUseMask.getBit(i))
                    {
                        t_storage[i].~Ts();
                    }
                }
                POOL_FREE(storage[type]);
                type++;
            }(),
            ... //
        );
        delete[] generations;
        delete[] storage;

        capacity = 0;
        storage = nullptr;
        inUseMask.resize(0);
        generations = nullptr;
    }
    ~MultiPoolImpl() { shutdown(); }

    template <class... ArgTypes>
    Handle<Ts...> insert(ArgTypes&&... args)
        requires(std::is_constructible_v<Ts, ArgTypes> && ...)
    {
        if(!inUseMask.anyBitClear())
        {
            if constexpr(isLimited)
            {
                if(capacity == limit)
                    return Handle<Ts...>::Null();
            }
            grow();
        }

        uint32_t index = inUseMask.getFirstBitClear();

        int i = 0;
        (
            [&]()
            {
                Ts* t_storage = static_cast<Ts*>(storage[i]);
                Ts* newT = new(&t_storage[index]) Ts(std::forward<ArgTypes>(args)); // placement new
                i++;
            }(),
            ... //
        );

        inUseMask.setBit(index);

        const Handle<Ts...> newHandle{index, generations[index]};
        assert(isHandleValid(newHandle));

        return newHandle;
    }

    void remove(Handle<Ts...> handle)
    {
        if(!isHandleValid(handle))
        {
            // todo: return boolean indicating nothing happened?
            return;
        }
        generations[handle.getIndex()]++;

        int i = 0;
        (
            [&]()
            {
                Ts* t_storage = static_cast<Ts*>(storage[i]);
                t_storage[handle.getIndex()].~Ts();
                i++;
            }(),
            ... //
        );

        inUseMask.clearBit(handle.getIndex());
    }

    // can retrieve a type thats not part of the parameter pack but is constructible from all pointers
    //  TODO: additionally allow construction from only a subset?
    template <typename R>
        requires std::is_constructible_v<R, std::add_pointer_t<Ts>...> && std::is_default_constructible_v<R>
    R get(Handle<Ts...> handle)
    {
        if(!isHandleValid(handle))
            return R{};

        return wrapPointers<R>(handle.getIndex(), std::make_index_sequence<sizeof...(Ts)>{});
    }

    template <typename T>
        requires PoolHelper::TypeInPack<T, Ts...>
    T* get(Handle<Ts...> handle)
    {
        if(!isHandleValid(handle))
            return nullptr;

        T* tStorage = static_cast<T*>(storage[PoolHelper::TypeIndex<T, Ts...>]);

        return &(tStorage[handle.getIndex()]);
    }

    Handle<Ts...> getFirst()
    {
        if(!inUseMask.anyBitSet())
            return Handle<Ts...>::Null();

        uint32_t index = inUseMask.getFirstBitSet();

        return {index, generations[index]};
    }

    bool isHandleValid(Handle<Ts...> handle)
    {
        assert(handle.getIndex() < capacity);

        return handle.getGeneration() == generations[handle.getIndex()];
    }

    template <typename... Args>
    Handle<Ts...> find(std::function<bool(std::add_pointer_t<Args>...)> pred)
        requires std::is_invocable<decltype(pred), std::add_pointer_t<Args>...>::value &&
                 (PoolHelper::TypeInPack<Args, Ts...> && ...)
    {
        for(uint32_t i = 0; i < capacity; i++)
        {
            if(inUseMask.getBit(i) && pred( //
                                          (&static_cast<Args*>(storage[PoolHelper::TypeIndex<Args, Ts...>])[i])...
                                          //
                                          ))
            {
                return Handle<Ts...>{i, generations[i]};
            }
        }
        return Handle<Ts...>::Null();
    }

    template <bool isConst = true>
    struct DirectIterator
    {
        using iterator_type = std::forward_iterator_tag;
        using difference_type = uint32_t;

        using Pool =
            std::conditional<isConst, const MultiPoolImpl<limit, Ts...>, MultiPoolImpl<limit, Ts...>>::type;

        DirectIterator(uint32_t start, Pool* owner) : pool(owner), index(start) {}

        // TODO: options to return all ptrs directly as tuple or something?
        Handle<Ts...> operator*() const { return {index, pool->generations[index]}; }

        DirectIterator& operator++()
        {
            // TODO: test if its faster to just for loop over all bits and get them here
            //       maybe with caching the current word
            // TODO: implement alternative iterator that caches full words of the freeLists bitmask?
            //       would be invalidated by insertions and removals (since cached mask != actual free mask),
            //       but faster
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
        capacity = std::min(limit, capacity * 2); // growing factor

        int i = 0;
        (
            [&]()
            {
                Ts* oldStorage = static_cast<Ts*>(storage[i]);

                storage[i] = static_cast<Ts*>(POOL_ALLOC(capacity * sizeof(Ts), alignof(Ts))); // NOLINT
                Ts* newStorage = static_cast<Ts*>(storage[i]);

                // if T is trivially_relocatable then we can grow the Pool with simple memmoves
                if constexpr(PoolHelper::is_trivially_relocatable<Ts>)
                {
                    memcpy(newStorage, oldStorage, oldCapacity * sizeof(Ts));
                }
                else
                {
                    for(int idx = 0; idx < oldCapacity; idx++)
                    {
                        // move construct from old storage into new
                        // C++20 has construct_at, just trying it out
                        std::construct_at(&newStorage[idx], std::move(oldStorage[idx]));
                        // explicitly destroy left over object in old storage
                        oldStorage[idx].~Ts();
                    }
                }

                POOL_FREE(oldStorage);

                i++;
            }(),
            ... //
        );

        uint32_t* oldGenerations = generations;
        generations = new uint32_t[capacity]; // NOLINT
        // todo: really just need to memset the part thats not memcpy-ed into afterwards
        memset(generations, 0u, 4 * capacity);
        memcpy(generations, oldGenerations, oldCapacity * sizeof(uint32_t));
        delete[] oldGenerations;

        inUseMask.resize(capacity);
        inUseMask.clear(oldCapacity, capacity - 1);
    }

    template <typename R, std::size_t... I>
        requires(sizeof...(Ts) == sizeof...(I))
    R wrapPointers(uint32_t index, std::index_sequence<I...>)
    {
        return R{&((Ts*)storage[I])[index]...};
    }

    static constexpr bool isLimited = limit != PoolHelper::unlimited;

    uint32_t capacity = 0;
    void** storage = nullptr;
    // bit set == element currently contains active object
    DynamicBitset inUseMask{0};
    uint32_t* generations = nullptr;
};

template <typename... Ts>
using MultiPool = MultiPoolImpl<PoolHelper::unlimited, Ts...>;

template <uint32_t limit, typename... Ts>
using MultiPoolLimited = MultiPoolImpl<limit, Ts...>;