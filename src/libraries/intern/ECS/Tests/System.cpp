#include <ECS/ECS.hpp>

int main()
{
    struct Foo
    {
    };

    struct Bar
    {
    };

    struct Baz
    {
    };

    ECS ecs;
    ecs.registerComponent<Foo>();
    ecs.registerComponent<Bar>();
    ecs.registerComponent<Baz>();

    auto entt = ecs.createEntity();
    entt.addComponent<Foo>();

    auto entt2 = ecs.createEntity();
    entt2.addComponent<Foo>();
    entt2.addComponent<Bar>();

    ecs.forEach<Foo>([](Foo* fp) { int x = 13; });
    ecs.forEach<Foo, Bar>(
        [](Foo* fp, Bar* bp)
        {
            int x = 13;
            int y = 27;
        });
    ecs.forEach<Bar, Foo>(
        [](Bar* bp2, Foo* fp2)
        {
            int x = 13;
            int y = 27;
        });
    ecs.forEach<Foo, Bar, Baz>([](Foo* fp, Bar* bp, Baz* bp2) {});
    ecs.forEach<Foo, Baz>([](Foo* fp, Baz* bp) {});
    ecs.forEach<Bar, Baz>([](Bar* bp, Baz* bp2) {});
}