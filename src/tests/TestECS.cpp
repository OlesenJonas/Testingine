#define TESTER_CLASS ECSTester
#include <intern/ECS/ECS.hpp>

#include <intern/Misc/Concepts.hpp>
#include <type_traits>

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
    static bool testInitialState(ECS& ecs)
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
        return true;
    }

    static bool testRegistration()
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
        TEST_EQUAL(ECS::ComponentTypeIDCache<Foo>::getID(), 0);
        // re-generating an ID should yield the same!
        TEST_EQUAL(ECS::ComponentTypeIDGenerator::Generate<Foo>(), 0);
        TEST_EQUAL(ECS::ComponentTypeIDCache<Foo>::getID(), 0);
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
        TEST_EQUAL(ECS::ComponentTypeIDCache<Bar>::getID(), 1);
        // re-generating an ID should yield the same!
        TEST_EQUAL(ECS::ComponentTypeIDGenerator::Generate<Bar>(), 1);
        TEST_EQUAL(ECS::ComponentTypeIDCache<Bar>::getID(), 1);
        // Everything else must be unchanged
        TEST_EQUAL(ecs.archetypes.size(), 1);
        TEST_EQUAL(ecs.archetypeLUT.size(), 1);
        TEST_EQUAL(ecs.entityLUT.size(), 0);
        TEST_EQUAL(ecs.archetypes[0].componentMask, emptyMask);
        TEST_EQUAL(ecs.archetypeLUT.find(emptyMask)->second, 0);

        // test component ID correct value, test cache gives correct value etc etc
        // test size correct value

        // test non trivially relocatable function

        return true;
    }

    static bool runTests()
    {
        bool success = true;
        success &= testRegistration();

        return success;
    };
};

int main()
{
    bool res = ECSTester::runTests();
    return res ? 0 : -1;
}