#include <Datastructures/Pool.hpp>

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
        ~A()
        {
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

    // test with small pool, fill all, assert that newley added object is at index (oldSize + 1)
    {
        Pool<A> pool1{2};
        pool1.insert(&count, uint32_t{13});
        pool1.insert(&count, Int{13}, 13);
        auto handle = pool1.insert(aInstance);
    }
    assert(count == 3);

    return 0;
}