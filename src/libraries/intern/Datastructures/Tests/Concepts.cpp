#include <Datastructures/Concepts.hpp>

int main()
{
    struct Foo
    {
    };
    struct Bar
    {
    };

    static_assert(isContained<Foo, Foo>::value);
    static_assert(!isContained<Foo>::value);
    static_assert(!isContained<Foo, Bar>::value);
    static_assert(!isContained<Foo, Bar, Bar>::value);
    static_assert(isContained<Foo, Bar, Bar, Foo>::value);
    static_assert(isContained<Foo, Foo, Bar, Bar>::value);
    static_assert(isContained<Foo, Foo, Bar, Bar>::value);

    static_assert(!isDistinct<Foo, Foo>::value);
    static_assert(isDistinct<Foo>::value);
    static_assert(isDistinct<Foo, Bar>::value);
    static_assert(!isDistinct<Foo, Bar, Bar>::value);
    static_assert(!isDistinct<Foo, Bar, Bar, Foo>::value);

    return 0;
}