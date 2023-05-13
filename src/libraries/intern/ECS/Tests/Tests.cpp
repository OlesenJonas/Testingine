#include <ECS/ECS.hpp>
#include <Testing/Check.hpp>
#include <unordered_set>
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
        bool res = CheckEqual(distinctValues.size(), 5);
        assert(res);
    }

    static void testInitialState(ECS& ecs)
    {
        bool res = true;
        res &= CheckEqual(ecs.entityIDCounter, 0);
        res &= CheckEqual(ecs.componentInfos.size(), 0);
        res &= CheckEqual(ecs.archetypes.size(), 1);
        res &= CheckEqual(ecs.archetypeLUT.size(), 1);
        res &= CheckEqual(ecs.entityLUT.size(), 0);
        ComponentMask emptyMask;
        res &= CheckEqual(emptyMask.none(), true);
        res &= CheckEqual(ecs.archetypes[0].componentMask, emptyMask);
        res &= CheckEqual(ecs.archetypeLUT.find(emptyMask)->second, 0);
        assert(res);
    }

    static void testRegistration()
    {
        bool res = true;
        ECS ecs;
        testInitialState(ecs);

        struct Foo
        {
            int x, y;
        };
        ecs.registerComponent<Foo>();

        // res &= CheckEqual(ecs.componentInfos.size(), 1);
        Check::Equal(ecs.componentInfos.size(), 1);
        ECS::ComponentInfo& info = ecs.componentInfos[0];
        res &= CheckEqual(info.size, sizeof(Foo));
        // Foo is trivially relocatable
        res &= CheckEqual(info.moveFunc, nullptr);
        res &= CheckEqual(info.destroyFunc, nullptr);
        res &= CheckEqual(ecs.bitmaskIndexFromComponentType<Foo>(), 0);
        // Everything else must be unchanged
        res &= CheckEqual(ecs.archetypes.size(), 1);
        res &= CheckEqual(ecs.archetypeLUT.size(), 1);
        res &= CheckEqual(ecs.entityLUT.size(), 0);
        ComponentMask emptyMask;
        res &= CheckEqual(emptyMask.none(), true);
        res &= CheckEqual(ecs.archetypes[0].componentMask, emptyMask);
        res &= CheckEqual(ecs.archetypeLUT.find(emptyMask)->second, 0);

        struct Bar
        {
            std::vector<Foo> foos;
        };
        // this shouldnt be trivially_relocatable
        static_assert(!ECSHelpers::is_trivially_relocatable<Bar>);
        ecs.registerComponent<Bar>();

        res &= CheckEqual(ecs.componentInfos.size(), 2);
        info = ecs.componentInfos[1];
        res &= CheckEqual(info.size, sizeof(Bar));
        // Foo is trivially relocatable
        res &= CheckNotEqual(info.moveFunc, nullptr);
        res &= CheckNotEqual(info.destroyFunc, nullptr);
        res &= CheckEqual(ecs.bitmaskIndexFromComponentType<Bar>(), 1);
        // Everything else must be unchanged
        res &= CheckEqual(ecs.archetypes.size(), 1);
        res &= CheckEqual(ecs.archetypeLUT.size(), 1);
        res &= CheckEqual(ecs.entityLUT.size(), 0);
        res &= CheckEqual(ecs.archetypes[0].componentMask, emptyMask);
        res &= CheckEqual(ecs.archetypeLUT.find(emptyMask)->second, 0);
    }

    static void testArchetypeCreation()
    {
        bool res = true;
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
        res &= CheckEqual(ecs.componentInfos.size(), 2);

        auto entt = ecs.createEntity();
        res &= CheckEqual(entt.getID(), 0);

        Foo foo{.x = rand(), .y = 13};
        entt.addComponent<Foo>(foo);

        ComponentMask fooMask;
        fooMask.set(ecs.bitmaskIndexFromComponentType<Foo>());
        res &= CheckEqual(ecs.entityLUT.size(), 1);
        res &= CheckEqual(ecs.archetypes.size(), 2);
        res &= CheckEqual(ecs.archetypeLUT.size(), 2);
        uint32_t fooArchetypeIndex = ecs.archetypeLUT.at(fooMask);
        res &= CheckEqual(fooArchetypeIndex, ecs.entityLUT.at(entt.getID()).archetypeIndex);
        ECS::Archetype* fooArchetype = &ecs.archetypes[fooArchetypeIndex];
        res &= CheckEqual(fooArchetype->componentMask, fooMask);
        res &= CheckEqual(fooArchetype->entityIDs.size(), 1);
        res &= CheckEqual(fooArchetype->entityIDs[0], entt.getID());
        res &= CheckEqual(fooArchetype->storageUsed, 1);
        Foo* gotFoo = entt.getComponent<Foo>();
        res &= CheckEqual(*gotFoo, foo);

        auto entt2 = ecs.createEntity();
        res &= CheckEqual(entt2.getID(), 1);
        entt2.addComponent<Bar>();
        // test bar only archetype
        ComponentMask barMask;
        barMask.set(ecs.bitmaskIndexFromComponentType<Bar>());
        res &= CheckEqual(ecs.entityLUT.size(), 2);
        res &= CheckEqual(ecs.archetypes.size(), 3);
        res &= CheckEqual(ecs.archetypeLUT.size(), 3);
        ECS::Archetype& barArchetype = ecs.archetypes[ecs.entityLUT.at(entt2.getID()).archetypeIndex];
        res &= CheckEqual(barArchetype.componentMask, barMask);
        res &= CheckEqual(barArchetype.entityIDs.size(), 1);
        res &= CheckEqual(barArchetype.entityIDs[0], entt2.getID());
        res &= CheckEqual(barArchetype.storageUsed, 1);
        Bar* gotBar = entt2.getComponent<Bar>();
        res &= CheckEqual(*gotBar, Bar{});

        entt.addComponent<Bar>();
        fooArchetype = &ecs.archetypes[fooArchetypeIndex];
        res &= CheckEqual(fooArchetype->entityIDs.size(), 0);
        res &= CheckEqual(fooArchetype->storageUsed, 0);
        // test foo+bar archetype
        ComponentMask foobarMask = fooMask | barMask;
        res &= CheckEqual(ecs.entityLUT.size(), 2);
        res &= CheckEqual(ecs.archetypes.size(), 4);
        res &= CheckEqual(ecs.archetypeLUT.size(), 4);
        ECS::Archetype& foobarArchetype = ecs.archetypes[ecs.entityLUT.at(entt.getID()).archetypeIndex];
        res &= CheckEqual(foobarArchetype.componentMask, foobarMask);
        res &= CheckEqual(foobarArchetype.entityIDs.size(), 1);
        res &= CheckEqual(foobarArchetype.entityIDs[0], entt.getID());
        res &= CheckEqual(foobarArchetype.storageUsed, 1);
        gotFoo = entt.getComponent<Foo>();
        gotBar = entt.getComponent<Bar>();
        res &= CheckEqual(*gotFoo, foo);
        res &= CheckEqual(*gotBar, Bar{});
    }

    struct Foo
    {
        int x, y;
        auto operator<=>(const Foo&) const = default;
    };
    static_assert(ECSHelpers::is_trivially_relocatable<Foo>);
    struct BarNR
    {
        std::vector<float> someVec;
        char someChar{};
        auto operator<=>(const BarNR&) const = default;
    };
    static_assert(!ECSHelpers::is_trivially_relocatable<BarNR>);

    static void testResizes()
    {
        bool res = true;
        ECS ecs;
        testInitialState(ecs);
        ecs.registerComponent<Foo>();
        ecs.registerComponent<BarNR>();

        std::vector<ECS::Entity> entts;
        std::vector<Foo> foos;
        std::vector<BarNR> bars;
        {
            auto& foo = foos.emplace_back(Foo{.x = rand(), .y = rand()});
            auto& bar =
                bars.emplace_back(BarNR{.someVec = {rand() * 0.1234f}, .someChar = static_cast<char>(rand())});
            // bar = Bar{.v = {rand() * 0.1234f}, .z = static_cast<char>(rand());
            ECS::Entity& entt = entts.emplace_back(ecs.createEntity());
            entt.addComponent<Foo>(foo);
            res &= CheckEqual(*entt.getComponent<Foo>(), foo);
            entt.addComponent<BarNR>(bar);
            res &= CheckEqual(*entt.getComponent<BarNR>(), bar);
        }
        uint32_t initialCapacity = ecs.archetypes[1].storageCapacity;
        for(int i = 1; i < initialCapacity; i++)
        {
            auto& foo = foos.emplace_back(Foo{.x = rand(), .y = rand()});
            auto& bar =
                bars.emplace_back(BarNR{.someVec = {rand() * 0.1234f}, .someChar = static_cast<char>(rand())});
            ECS::Entity& entt = entts.emplace_back(ecs.createEntity());
            entt.addComponent<Foo>(foo);
            res &= CheckEqual(*entt.getComponent<Foo>(), foo);
            entt.addComponent<BarNR>(bar);
            res &= CheckEqual(*entt.getComponent<BarNR>(), bar);
        }

        for(int i = 0; i < 10; i++)
        {
            res &= CheckEqual(*entts[i].getComponent<Foo>(), foos[i]);
            res &= CheckEqual(*entts[i].getComponent<BarNR>(), bars[i]);
        }

        res &= CheckEqual(ecs.archetypes[0].storageUsed, 0);
        res &= CheckEqual(ecs.archetypes[1].storageUsed, 0);
        res &= CheckEqual(ecs.archetypes[2].storageUsed, ecs.archetypes[2].storageCapacity);
        // Force growing storage
        {
            auto& foo = foos.emplace_back(Foo{.x = rand(), .y = rand()});
            auto& bar =
                bars.emplace_back(BarNR{.someVec = {rand() * 0.1234f}, .someChar = static_cast<char>(rand())});
            ECS::Entity& entt = entts.emplace_back(ecs.createEntity());
            entt.addComponent<Foo>(foo);
            res &= CheckEqual(*entt.getComponent<Foo>(), foo);
            entt.addComponent<BarNR>(bar);
            res &= CheckEqual(*entt.getComponent<BarNR>(), bar);
        }

        for(int i = 0; i < 11; i++)
        {
            res &= CheckEqual(*entts[i].getComponent<Foo>(), foos[i]);
            res &= CheckEqual(*entts[i].getComponent<BarNR>(), bars[i]);
        }

        res &= CheckNotEqual(ecs.archetypes[2].storageCapacity, initialCapacity);

        const uint32_t deletedIndex = 5;
        res &= CheckEqual(*entts[deletedIndex].getComponent<BarNR>(), bars[deletedIndex]);
        entts[deletedIndex].removeComponent<Foo>();
        res &= CheckEqual(*entts[deletedIndex].getComponent<BarNR>(), bars[deletedIndex]);
        entts[deletedIndex].removeComponent<BarNR>();

        const uint32_t deletedIndex2 = 7;
        res &= CheckEqual(*entts[deletedIndex2].getComponent<Foo>(), foos[deletedIndex2]);
        entts[deletedIndex2].removeComponent<BarNR>();
        res &= CheckEqual(*entts[deletedIndex2].getComponent<Foo>(), foos[deletedIndex2]);
        entts[deletedIndex2].removeComponent<Foo>();

        for(int i = 0; i < entts.size(); i++)
        {
            if(i != deletedIndex && i != deletedIndex2)
            {
                res &= CheckEqual(*entts[i].getComponent<Foo>(), foos[i]);
                res &= CheckEqual(*entts[i].getComponent<BarNR>(), bars[i]);
            }
        }
        entts[entts.size() - 1].removeComponent<Foo>();
        entts[entts.size() - 1].removeComponent<BarNR>();
        entts.pop_back();

        for(int i = 0; i < entts.size(); i++)
        {
            if(i != deletedIndex && i != deletedIndex2)
            {
                res &= CheckEqual(*entts[i].getComponent<Foo>(), foos[i]);
                res &= CheckEqual(*entts[i].getComponent<BarNR>(), bars[i]);
            }
        }
        assert(res);
    }

    static void testResizes2()
    {
        bool res = true;
        // The same as testResizes() but this time the order of components being removed/added
        // is swapped (registering them still happens in the same order, so that their bitmask indices are the
        // same as in testResizes)
        ECS ecs;
        testInitialState(ecs);
        ecs.registerComponent<Foo>();
        ecs.registerComponent<BarNR>();

        std::vector<ECS::Entity> entts;
        std::vector<Foo> foos;
        std::vector<BarNR> bars;
        {
            auto& foo = foos.emplace_back(Foo{.x = rand(), .y = rand()});
            auto& bar =
                bars.emplace_back(BarNR{.someVec = {rand() * 0.1234f}, .someChar = static_cast<char>(rand())});
            // bar = Bar{.v = {rand() * 0.1234f}, .z = static_cast<char>(rand());
            ECS::Entity& entt = entts.emplace_back(ecs.createEntity());
            entt.addComponent<BarNR>(bar);
            res &= CheckEqual(*entt.getComponent<BarNR>(), bar);
            entt.addComponent<Foo>(foo);
            res &= CheckEqual(*entt.getComponent<Foo>(), foo);
        }
        uint32_t initialCapacity = ecs.archetypes[1].storageCapacity;
        for(int i = 1; i < initialCapacity; i++)
        {
            auto& foo = foos.emplace_back(Foo{.x = rand(), .y = rand()});
            auto& bar =
                bars.emplace_back(BarNR{.someVec = {rand() * 0.1234f}, .someChar = static_cast<char>(rand())});
            ECS::Entity& entt = entts.emplace_back(ecs.createEntity());
            entt.addComponent<BarNR>(bar);
            res &= CheckEqual(*entt.getComponent<BarNR>(), bar);
            entt.addComponent<Foo>(foo);
            res &= CheckEqual(*entt.getComponent<Foo>(), foo);
        }

        for(int i = 0; i < 10; i++)
        {
            res &= CheckEqual(*entts[i].getComponent<BarNR>(), bars[i]);
            res &= CheckEqual(*entts[i].getComponent<Foo>(), foos[i]);
        }

        res &= CheckEqual(ecs.archetypes[0].storageUsed, 0);
        res &= CheckEqual(ecs.archetypes[1].storageUsed, 0);
        res &= CheckEqual(ecs.archetypes[2].storageUsed, ecs.archetypes[2].storageCapacity);
        // Force growing storage
        {
            auto& foo = foos.emplace_back(Foo{.x = rand(), .y = rand()});
            auto& bar =
                bars.emplace_back(BarNR{.someVec = {rand() * 0.1234f}, .someChar = static_cast<char>(rand())});
            ECS::Entity& entt = entts.emplace_back(ecs.createEntity());
            entt.addComponent<BarNR>(bar);
            res &= CheckEqual(*entt.getComponent<BarNR>(), bar);
            entt.addComponent<Foo>(foo);
            res &= CheckEqual(*entt.getComponent<Foo>(), foo);
        }

        for(int i = 0; i < 11; i++)
        {
            res &= CheckEqual(*entts[i].getComponent<BarNR>(), bars[i]);
            res &= CheckEqual(*entts[i].getComponent<Foo>(), foos[i]);
        }

        res &= CheckNotEqual(ecs.archetypes[2].storageCapacity, initialCapacity);

        const uint32_t deletedIndex = 5;
        res &= CheckEqual(*entts[deletedIndex].getComponent<Foo>(), foos[deletedIndex]);
        entts[deletedIndex].removeComponent<BarNR>();
        res &= CheckEqual(*entts[deletedIndex].getComponent<Foo>(), foos[deletedIndex]);
        entts[deletedIndex].removeComponent<Foo>();

        for(int i = 0; i < entts.size(); i++)
        {
            if(i != deletedIndex)
            {
                res &= CheckEqual(*entts[i].getComponent<BarNR>(), bars[i]);
                res &= CheckEqual(*entts[i].getComponent<Foo>(), foos[i]);
            }
        }
        entts[entts.size() - 1].removeComponent<BarNR>();
        entts[entts.size() - 1].removeComponent<Foo>();
        entts.pop_back();

        for(int i = 0; i < entts.size(); i++)
        {
            if(i != deletedIndex)
            {
                res &= CheckEqual(*entts[i].getComponent<BarNR>(), bars[i]);
                res &= CheckEqual(*entts[i].getComponent<Foo>(), foos[i]);
            }
        }
        assert(res);
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

        bool res = true;
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
            res &= CheckEqual(*entts[i].getComponent<Foo>(), foo);
        }
        for(int i = 0; i < 10; i++)
        {
            auto& bar = bars.emplace_back(Bar{.x = rand() * 0.1234f, .z = static_cast<char>(rand())});
            entts[i].addComponent<Bar>(bar);
            res &= CheckEqual(*entts[i].getComponent<Bar>(), bar);
            // check that elements that are still left in just foo archetype still have correct values
            for(int j = i + 1; j < 10; j++)
            {
                res &= CheckEqual(*entts[j].getComponent<Foo>(), foos[j]);
            }
        }
        assert(res);
    }

    static void runTests()
    {
        testKeyGen();
        testRegistration();
        testRegistration();
        testArchetypeCreation();
        testResizes();
        testResizes2();
        fillTest();
    };
};

int main()
{
    ECSTester::runTests();
    return 0;
}