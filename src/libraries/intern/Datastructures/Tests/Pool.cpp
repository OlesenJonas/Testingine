#include <Datastructures/Pool/Pool.hpp>

#include <array>
#include <vector>

inline void abandonIfFalse(bool b, int code = -1)
{
    if(!b)
    {
        exit(code);
    }
}

int main()
{
    int count = 0;
    struct Int
    {
        uint32_t i;
    };
    struct A
    {
        A(int* p, uint32_t b) : counter(p), a(b){};
        A(int* p, Int c, uint16_t d) : counter(p), a(c.i + d){};
        A(A& other)
        {
            a = other.a;
            counter = other.counter;
        }
        A(A&& other)
        {
            a = other.a;
            counter = other.counter;
            other.counter = nullptr;
        }
        ~A()
        {
            if(counter != nullptr)
                (*counter)++;
        };
        int a;
        int* counter;
    };
    A aInstance{&count, 33};
    std::vector<A> a;
    a.emplace_back(&count, Int{3}, 4);
    a.emplace_back(aInstance);
    count = 0;

    // this does not grow the pool
    {
        Pool<A> pool1{2};
        pool1.insert(&count, uint32_t{13});
        pool1.insert(&count, Int{13}, 13);
    }
    assert(count == 2);

    count = 0;

    {
        Pool<A> pool1{2};
        pool1.insert(&count, uint32_t{13});
        pool1.insert(&count, Int{13}, 13);
        auto handle = pool1.insert(aInstance);
    }
    assert(count == 3);

    {
        PoolLimited<2, float> pool1{2u};
        auto fstHandle = pool1.insert(3.0);
        assert(fstHandle.isNonNull());
        pool1.insert(2.0);
        auto handle = pool1.insert(1.0);
        assert(!handle.isNonNull());
    }
    assert(count == 3);

    // Testing iterators
    {
        Pool<float> pool1{2u};

        auto handle1 = pool1.insert(3.0);
        auto handle2 = pool1.insert(2.0);
        auto handle3 = pool1.insert(1.0);

        for(auto num : pool1)
        {
            (*num)++;
        }

        assert(*pool1.get(handle1) == 4.0f);
        assert(*pool1.get(handle2) == 3.0f);
        assert(*pool1.get(handle3) == 2.0f);

        decltype(pool1)::DirectIterator<false> deletedIter{1, &pool1};

        pool1.remove(handle2);

        for(auto* num : pool1)
        {
            assert(*num != 3.0f);
        }
        int count = 0;
        for(auto iter = pool1.begin(); iter != pool1.end(); iter++)
        {
            assert(iter != deletedIter);
            count++;
        }
        assert(count == 2);
    }

    return 0;
}