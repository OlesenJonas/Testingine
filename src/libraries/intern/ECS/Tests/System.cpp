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

    ecs.forEach<Foo>([](Foo* fp) {});
    ecs.forEach<Foo, Bar>([](Foo* fp, Bar* bp) {});
    ecs.forEach<Foo, Bar, Baz>([](Foo* fp, Bar* bp, Baz* bp2) {});
    ecs.forEach<Foo, Baz>([](Foo* fp, Baz* bp) {});
    ecs.forEach<Bar, Baz>([](Bar* bp, Baz* bp2) {});
}