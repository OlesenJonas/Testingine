#include <ECS/ECS.hpp>
#include <Testing/Check.hpp>

struct Foo
{
    const char* name = "Foo";
    int i = 0;
    auto operator<=>(const Foo&) const = default;
};

struct Bar
{
    const char* name = "Bar";
    int i = 0;
    auto operator<=>(const Bar&) const = default;
};

struct Baz
{
};

void compileTest()
{

    ECS ecs;
    ecs.registerComponent<Foo>();
    ecs.registerComponent<Bar>();
    ecs.registerComponent<Baz>();

    auto entt = ecs.createEntity();
    entt.addComponent<Foo>();

    auto entt2 = ecs.createEntity();
    entt2.addComponent<Foo>();
    entt2.addComponent<Bar>();

    ecs.forEach<Foo>([](Foo* fp, uint32_t count) { int x = 13; });
    ecs.forEach<Foo, Bar>(
        [](Foo* fp, Bar* bp, uint32_t count)
        {
            int x = 13;
            int y = 27;
        });
    ecs.forEach<Bar, Foo>(
        [](Bar* bp2, Foo* fp2, uint32_t count)
        {
            int x = 13;
            int y = 27;
        });
    ecs.forEach<Foo, Bar, Baz>([](Foo* fp, Bar* bp, Baz* bp2, uint32_t count) {});
    ecs.forEach<Foo, Baz>([](Foo* fp, Baz* bp, uint32_t count) {});
    ecs.forEach<Bar, Baz>([](Bar* bp, Baz* bp2, uint32_t count) {});
}

void testChange()
{
    ECS ecs;
    ecs.registerComponent<Baz>();
    ecs.registerComponent<Foo>();
    ecs.registerComponent<Bar>();

    std::vector<ECS::Entity> fooOnlyEntts;
    std::vector<Foo> foosOnly;
    for(int i = 0; i < 11; i++)
    {
        auto& entt = fooOnlyEntts.emplace_back(ecs.createEntity());
        auto& foo = foosOnly.emplace_back(Foo{.i = i});
        entt.addComponent<Foo>(foo);
    }

    std::vector<ECS::Entity> fooAndBarEntts;
    std::vector<Foo> fooAndBarFoos;
    std::vector<Bar> fooAndBarBars;
    for(int i = 10; i < 21; i++)
    {
        auto& entt = fooAndBarEntts.emplace_back(ecs.createEntity());
        auto& bar = fooAndBarBars.emplace_back(Bar{.i = 2 * i});
        auto& foo = fooAndBarFoos.emplace_back(Foo{.i = i});
        entt.addComponent<Bar>(bar);
        entt.addComponent<Foo>(foo);
    }

    ecs.forEach<Foo>(
        [](Foo* fp, uint32_t count)
        {
            for(int i = 0; i < count; i++)
            {
                fp[i].i += 3;
            }
        });

    bool res = true;
    for(int i = 0; i < fooOnlyEntts.size(); i++)
    {
        auto& entt = fooOnlyEntts[i];
        Foo& oldFoo = foosOnly[i];
        Foo* newFoo = entt.getComponent<Foo>();
        res &= CheckEqual(oldFoo.i + 3, newFoo->i);
        oldFoo.i += 3;
        res &= CheckEqual(oldFoo.i, newFoo->i);
    }
    for(int i = 0; i < fooAndBarEntts.size(); i++)
    {
        auto& entt = fooAndBarEntts[i];
        Foo& oldFoo = fooAndBarFoos[i];
        Foo* newFoo = entt.getComponent<Foo>();
        res &= CheckEqual(oldFoo.i + 3, newFoo->i);
        oldFoo.i += 3;
        res &= CheckEqual(oldFoo.i, newFoo->i);
    }
    assert(res);

    ecs.forEach<Foo, Bar>(
        [](Foo* fp, Bar* bp, uint32_t count)
        {
            for(int i = 0; i < count; i++)
            {
                fp[i].name = "FooAndBar";
            }
        });
    // foo only entities should be unchaged
    for(int i = 0; i < fooOnlyEntts.size(); i++)
    {
        auto& entt = fooOnlyEntts[i];
        Foo& oldFoo = foosOnly[i];
        Foo* newFoo = entt.getComponent<Foo>();
        res &= CheckEqual(oldFoo.i, newFoo->i);
        res &= CheckEqual(strcmp(newFoo->name, "Foo"), 0);
    }
    assert(res);

    ecs.forEach<Bar, Foo>(
        [](Bar* bp, Foo* fp, uint32_t count)
        {
            for(int i = 0; i < count; i++)
            {
                bp[i].i += 2 * fp[i].i;
            }
        });
    for(int i = 0; i < fooAndBarEntts.size(); i++)
    {
        auto& entt = fooAndBarEntts[i];
        Foo& oldFoo = fooAndBarFoos[i];
        Bar& oldBar = fooAndBarBars[i];
        Foo* newFoo = entt.getComponent<Foo>();
        Bar* newBar = entt.getComponent<Bar>();
        res &= CheckEqual(oldFoo.i, newFoo->i);
        res &= CheckEqual(oldBar.i + 2 * oldFoo.i, newBar->i);
        res &= CheckEqual(strcmp(newFoo->name, "FooAndBar"), 0);
        oldFoo.i += 3;
    }
    assert(res);

    // The entities holding only Foo should be unchanged from the forEach<Foo,Bar>
    for(int i = 0; i < fooOnlyEntts.size(); i++)
    {
        auto& entt = fooOnlyEntts[i];
        Foo& oldFoo = foosOnly[i];
        Foo* newFoo = entt.getComponent<Foo>();
        res &= CheckEqual(oldFoo.i, newFoo->i);
        res &= CheckEqual(strcmp(newFoo->name, "Foo"), 0);
        oldFoo.i += 3;
    }

    // CHeck only foo still same!!
}

int main()
{
    compileTest();
    testChange();
}