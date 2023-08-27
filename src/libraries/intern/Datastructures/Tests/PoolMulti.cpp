#include <Datastructures/Pool/PoolMulti.hpp>
#include <Testing/Check.hpp>

#include <array>
#include <vector>

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
        MultiPool<A> pool1{2};
        pool1.insert(A{&count, uint32_t{13}});
        pool1.insert(A{&count, Int{13}, 13});
    }
    assert(count == 2);

    count = 0;

    // this does grow the pool
    {
        MultiPool<A> pool1{2};
        auto handle0 = pool1.insert(A{&count, uint32_t{13}});
        auto handle1 = pool1.insert(A{&count, Int{13}, 13});
        auto handle2 = pool1.insert(aInstance);
    }
    assert(count == 3);

    {
        MultiPoolLimited<2, float> pool1{2u};
        auto fstHandle = pool1.insert(3.0);
        assert(fstHandle.isValid());
        pool1.insert(2.0);
        auto handle = pool1.insert(1.0);
        assert(!handle.isValid());
    }
    assert(count == 3);

    // Test get()
    {
        MultiPool<float> pool{2};
        auto handle = pool.insert(3.0f);
        struct Foo
        {
            float* fp;
        };
        Foo foo = pool.get<Foo>(handle);
        auto* fp = pool.get<float*>(handle);
    }
    {
        MultiPool<float, char> pool{2};
        auto handle0 = pool.insert(3.0f, 'a');
        auto handle1 = pool.insert(5.0f, 'x');
        auto handle2 = pool.insert(11.6f, 'd');
        struct Foo
        {
            float* fp;
            char* cp;
        };
        Foo foo = pool.get<Foo>(handle0);
        assert(*foo.fp == 3.0f);
        assert(*foo.cp == 'a');
        auto* fp = pool.get<float>(handle1);
        auto* cp = pool.get<char>(handle1);
        assert(*fp == 5.0f);
        assert(*cp == 'x');
    }

    // Testing iterators
    {
        MultiPool<float, int> pool{2u};

        auto handle1 = pool.insert(3.0, 1);
        auto handle2 = pool.insert(2.0, 2);
        auto handle3 = pool.insert(1.0, 3);

        for(auto handle : pool)
        {
            *pool.get<float>(handle) += 1.0f;
            *pool.get<int>(handle) -= 2.0f;
        }

        assert(*pool.get<float>(handle1) == 4.0f);
        assert(*pool.get<float>(handle2) == 3.0f);
        assert(*pool.get<float>(handle3) == 2.0f);
        assert(*pool.get<int>(handle1) == -1);
        assert(*pool.get<int>(handle2) == 0);
        assert(*pool.get<int>(handle3) == 1);

        decltype(pool)::DirectIterator<false> deletedIter{1, &pool};

        pool.remove(handle2);

        for(auto handle : pool)
        {
            assert(*pool.get<float>(handle) != 3.0f);
        }
        int count = 0;
        for(auto iter = pool.begin(); iter != pool.end(); iter++)
        {
            assert(iter != deletedIter);
            count++;
        }
        assert(count == 2);
    }

    // Testig find

    {
        MultiPool<float, char> pool{2};
        auto handle0 = pool.insert(3.0f, 'a');
        auto handle1 = pool.insert(5.0f, 'x');
        auto handle2 = pool.insert(11.6f, 'd');

        auto found0 = pool.find<float>([&](float* f) -> bool { return *f == 3.0f; });
        assert(found0 == handle0);
    }
    {
        MultiPool<float, char> pool{2};
        auto handle0 = pool.insert(3.0f, 'a');
        auto handle1 = pool.insert(5.0f, 'x');
        auto handle2 = pool.insert(3.0f, 'd');

        auto found0 = pool.find<float, char>([&](float* f, char* c) -> bool { return *f == 3.0f && *c == 'd'; });
        assert(found0 == handle2);
    }
    {
        MultiPool<float, char> pool{2};
        auto handle0 = pool.insert(5.0f, 'a');
        auto handle1 = pool.insert(3.0f, 'a');
        auto handle2 = pool.insert(5.0f, 'd');

        auto found0 = pool.find<char, float>([&](char* c, float* f) -> bool { return *f == 3.0f && *c == 'a'; });
        assert(found0 == handle1);
    }
    {
        struct Foo
        {
            double d;
        };
        MultiPool<Foo, float, char, int> pool{2};
        auto handle0 = pool.insert(7.0, 5.0f, 'a', 33);
        auto handle1 = pool.insert(7.0, 3.0f, 'a', 44);
        auto handle2 = pool.insert(8.0, 5.0f, 'd', 55);

        auto found0 = pool.find<Foo, int>([&](Foo* f, int* i) -> bool { return f->d == 7.0 && *i == 33; });
        assert(found0 == handle0);
    }

    return 0;
}