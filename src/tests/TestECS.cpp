#define TESTER_CLASS ECSTester
#include <intern/ECS/ECS.hpp>

#include <intern/Misc/Concepts.hpp>
#include <type_traits>
#include <unordered_set>

// could also implement an assertEqualExact(?) that requires exact T matches
template <typename T1, typename T2>
    requires std::is_convertible<T2, T1>::value
inline void assertEqual(T1 left, T2 right, const char* file = "", int line = -1)
{
    if(left != right)
    {
        // very useful if testing is done with debug builds
        printf("assertEqual failed: %s line %d \n", file, line);
        assert(false);
        exit(-1);
    }
}
template <typename T1, typename T2>
    requires std::is_convertible<T2, T1>::value
inline void assertNotEqual(T1 left, T2 right, const char* file = "", int line = -1)
{
    if(left == right)
    {
        // very useful if testing is done with debug builds
        printf("assertNotEqual failed: %s line %d \n", file, line);
        assert(false);
        exit(-1);
    }
}
#define TEST_EQUAL(x, y) assertEqual(x, y, __FILE__, __LINE__)
#define TEST_NOT_EQUAL(x, y) assertNotEqual(x, y, __FILE__, __LINE__)

/*
    These tests werent created systematically and are definitly not exhaustive!
*/
class ECSTester
{
  public:
    static void testKeyGen()
    {
        using ECSHelpers::TypeKey;
        struct Foo
        {
        };
        struct Bar
        {
        };
        std::unordered_set<TypeKey> distinctValues;
        distinctValues.insert(ECSHelpers::getTypeKey<int>());
        distinctValues.insert(ECSHelpers::getTypeKey<double>());
        distinctValues.insert(ECSHelpers::getTypeKey<Foo>());
        distinctValues.insert(ECSHelpers::getTypeKey<Bar>());
        distinctValues.insert(ECSHelpers::getTypeKey<decltype(ECSTester::testKeyGen)>());
        TEST_EQUAL(distinctValues.size(), 5);
    }

    static void testInitialState(ECS& ecs)
    {
        TEST_EQUAL(ecs.entityIDCounter, 0);
        TEST_EQUAL(ecs.componentInfos.size(), 0);
        TEST_EQUAL(ecs.archetypes.size(), 1);
        TEST_EQUAL(ecs.archetypeLUT.size(), 1);
        TEST_EQUAL(ecs.entityLUT.size(), 0);
        ComponentMask emptyMask;
        TEST_EQUAL(emptyMask.none(), true);
        TEST_EQUAL(ecs.archetypes[0].componentMask, emptyMask);
        TEST_EQUAL(ecs.archetypeLUT.find(emptyMask)->second, 0);
    }

    static void testRegistration()
    {
        ECS ecs;
        testInitialState(ecs);

        struct Foo
        {
            int x, y;
        };
        ecs.registerComponent<Foo>();

        TEST_EQUAL(ecs.componentInfos.size(), 1);
        ECS::ComponentInfo& info = ecs.componentInfos[0];
        TEST_EQUAL(info.size, sizeof(Foo));
        // Foo is trivially relocatable
        TEST_EQUAL(info.moveFunc, nullptr);
        TEST_EQUAL(info.destroyFunc, nullptr);
        TEST_EQUAL(ecs.bitmaskIndexFromComponentType<Foo>(), 0);
        // Everything else must be unchanged
        TEST_EQUAL(ecs.archetypes.size(), 1);
        TEST_EQUAL(ecs.archetypeLUT.size(), 1);
        TEST_EQUAL(ecs.entityLUT.size(), 0);
        ComponentMask emptyMask;
        TEST_EQUAL(emptyMask.none(), true);
        TEST_EQUAL(ecs.archetypes[0].componentMask, emptyMask);
        TEST_EQUAL(ecs.archetypeLUT.find(emptyMask)->second, 0);

        struct Bar
        {
            std::vector<Foo> foos;
        };
        // this shouldnt be trivially_relocatable
        static_assert(!is_trivially_relocatable<Bar>);
        ecs.registerComponent<Bar>();

        TEST_EQUAL(ecs.componentInfos.size(), 2);
        info = ecs.componentInfos[1];
        TEST_EQUAL(info.size, sizeof(Bar));
        // Foo is trivially relocatable
        TEST_NOT_EQUAL(info.moveFunc, nullptr);
        TEST_NOT_EQUAL(info.destroyFunc, nullptr);
        TEST_EQUAL(ecs.bitmaskIndexFromComponentType<Bar>(), 1);
        // Everything else must be unchanged
        TEST_EQUAL(ecs.archetypes.size(), 1);
        TEST_EQUAL(ecs.archetypeLUT.size(), 1);
        TEST_EQUAL(ecs.entityLUT.size(), 0);
        TEST_EQUAL(ecs.archetypes[0].componentMask, emptyMask);
        TEST_EQUAL(ecs.archetypeLUT.find(emptyMask)->second, 0);
    }

    static void testArchetypeCreation()
    {
        ECS ecs;
        testInitialState(ecs);

        struct Foo
        {
            int x, y;
            auto operator<=>(const Foo&) const = default;
        };
        ecs.registerComponent<Foo>();
        struct Bar
        {
            std::vector<int> v;
            auto operator<=>(const Bar&) const = default;
        };
        ecs.registerComponent<Bar>();
        TEST_EQUAL(ecs.componentInfos.size(), 2);

        auto entt = ecs.createEntity();
        TEST_EQUAL(entt.getID(), 0);

        Foo foo{.x = rand(), .y = 13};
        entt.addComponent<Foo>(foo);

        ComponentMask fooMask;
        fooMask.set(ecs.bitmaskIndexFromComponentType<Foo>());
        TEST_EQUAL(ecs.entityLUT.size(), 1);
        TEST_EQUAL(ecs.archetypes.size(), 2);
        TEST_EQUAL(ecs.archetypeLUT.size(), 2);
        uint32_t fooArchetypeIndex = ecs.archetypeLUT.at(fooMask);
        TEST_EQUAL(fooArchetypeIndex, ecs.entityLUT.at(entt.getID()).archetypeIndex);
        ECS::Archetype* fooArchetype = &ecs.archetypes[fooArchetypeIndex];
        TEST_EQUAL(fooArchetype->componentMask, fooMask);
        TEST_EQUAL(fooArchetype->entityIDs.size(), 1);
        TEST_EQUAL(fooArchetype->entityIDs[0], entt.getID());
        TEST_EQUAL(fooArchetype->storageUsed, 1);
        Foo* gotFoo = entt.getComponent<Foo>();
        TEST_EQUAL(*gotFoo, foo);

        auto entt2 = ecs.createEntity();
        TEST_EQUAL(entt2.getID(), 1);
        entt2.addComponent<Bar>();
        // test bar only archetype
        ComponentMask barMask;
        barMask.set(ecs.bitmaskIndexFromComponentType<Bar>());
        TEST_EQUAL(ecs.entityLUT.size(), 2);
        TEST_EQUAL(ecs.archetypes.size(), 3);
        TEST_EQUAL(ecs.archetypeLUT.size(), 3);
        ECS::Archetype& barArchetype = ecs.archetypes[ecs.entityLUT.at(entt2.getID()).archetypeIndex];
        TEST_EQUAL(barArchetype.componentMask, barMask);
        TEST_EQUAL(barArchetype.entityIDs.size(), 1);
        TEST_EQUAL(barArchetype.entityIDs[0], entt2.getID());
        TEST_EQUAL(barArchetype.storageUsed, 1);
        Bar* gotBar = entt2.getComponent<Bar>();
        TEST_EQUAL(*gotBar, Bar{});

        entt.addComponent<Bar>();
        fooArchetype = &ecs.archetypes[fooArchetypeIndex];
        TEST_EQUAL(fooArchetype->entityIDs.size(), 0);
        TEST_EQUAL(fooArchetype->storageUsed, 0);
        // test foo+bar archetype
        ComponentMask foobarMask = fooMask | barMask;
        TEST_EQUAL(ecs.entityLUT.size(), 2);
        TEST_EQUAL(ecs.archetypes.size(), 4);
        TEST_EQUAL(ecs.archetypeLUT.size(), 4);
        ECS::Archetype& foobarArchetype = ecs.archetypes[ecs.entityLUT.at(entt.getID()).archetypeIndex];
        TEST_EQUAL(foobarArchetype.componentMask, foobarMask);
        TEST_EQUAL(foobarArchetype.entityIDs.size(), 1);
        TEST_EQUAL(foobarArchetype.entityIDs[0], entt.getID());
        TEST_EQUAL(foobarArchetype.storageUsed, 1);
        gotFoo = entt.getComponent<Foo>();
        gotBar = entt.getComponent<Bar>();
        TEST_EQUAL(*gotFoo, foo);
        TEST_EQUAL(*gotBar, Bar{});
    }

    static void testResizes()
    {

        struct Foo
        {
            int x, y;
            auto operator<=>(const Foo&) const = default;
        };
        struct Bar
        {
            float x;
            char z;
            auto operator<=>(const Bar&) const = default;
        };

        ECS ecs;
        testInitialState(ecs);
        ecs.registerComponent<Foo>();
        ecs.registerComponent<Bar>();

        std::vector<ECS::Entity> entts;
        std::vector<Foo> foos;
        std::vector<Bar> bars;
        for(int i = 0; i < 10; i++)
        {
            auto& foo = foos.emplace_back(Foo{.x = rand(), .y = rand()});
            auto& bar = bars.emplace_back(Bar{.x = rand() * 0.1234f, .z = static_cast<char>(rand())});
            ECS::Entity& entt = entts.emplace_back(ecs.createEntity());
            entt.addComponent<Foo>(foo);
            TEST_EQUAL(*entt.getComponent<Foo>(), foo);
            entt.addComponent<Bar>(bar);
            TEST_EQUAL(*entt.getComponent<Bar>(), bar);
        }

        for(int i = 0; i < 10; i++)
        {
            TEST_EQUAL(*entts[i].getComponent<Foo>(), foos[i]);
            TEST_EQUAL(*entts[i].getComponent<Bar>(), bars[i]);
        }

        /*
            todo:
                - add one more entt so that grow() happens, check if still same
                - remove from middle so that fixGap happens, check if still same
                - remove from back, check if still same
        */
    }

    static void fillTest()
    {
        struct Foo
        {
            int x, y;
            auto operator<=>(const Foo&) const = default;
        };
        struct Bar
        {
            float x;
            char z;
            auto operator<=>(const Bar&) const = default;
        };

        ECS ecs;
        testInitialState(ecs);
        ecs.registerComponent<Foo>();
        ecs.registerComponent<Bar>();

        std::vector<ECS::Entity> entts;
        std::vector<Foo> foos;
        std::vector<Bar> bars;
        for(int i = 0; i < 10; i++)
        {
            ECS::Entity& entt = entts.emplace_back(ecs.createEntity());
        }
        for(int i = 0; i < 10; i++)
        {
            auto& foo = foos.emplace_back(Foo{.x = rand(), .y = rand()});
            entts[i].addComponent<Foo>(foo);
            TEST_EQUAL(*entts[i].getComponent<Foo>(), foo);
        }
        for(int i = 0; i < 10; i++)
        {
            auto& bar = bars.emplace_back(Bar{.x = rand() * 0.1234f, .z = static_cast<char>(rand())});
            entts[i].addComponent<Bar>(bar);
            TEST_EQUAL(*entts[i].getComponent<Bar>(), bar);
        }
    }

    static void runTests()
    {
        testKeyGen();
        testRegistration();
        testRegistration();
        testArchetypeCreation();
        testResizes();
        fillTest();
    };
};

int main()
{
    ECSTester::runTests();
    return 0;
}