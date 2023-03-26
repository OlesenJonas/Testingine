#include <initializer_list>
#include <intern/Datastructures/Span.hpp>

#include <type_traits>
#include <vector>

inline void abandonIfFalse(bool b, int code = -1)
{
    if(!b)
    {
        exit(code);
    }
}

template <typename T>
void processSpan(Span<T> floats)
{
}

int main()
{
    // todo: more extensive testing, edge cases, errors cases...

    //  testing constructors + makeSpan

    // empty
    {
        Span<int> s;
        abandonIfFalse(s.data() == nullptr);
        abandonIfFalse(s.empty());
        abandonIfFalse(s.size() == 0);
    }

    {
        struct Info
        {
            struct Stage
            {
                Stage(int s) : stage(s){};
                int stage;
            };
            const Span<const Stage> stages;
        };

        Info i{.stages = std::initializer_list<Info::Stage>{1, 2, 3}};
        Info i2{.stages = {1, 2, 3}};
    }

    // from initializer list
    {
        // this must _not_ work, but no setup currently to explicitly test for compiler failure
        // processSpan<float>({1.0f, 2.0f, 3.0f});

        // this must work
        processSpan<const float>({1.0f, 2.0f, 3.0f});

        struct S
        {
            int x = 13;
        };
        const S s1;
        const S s2;
        processSpan<const S>({s1, s2});

        S s3;
        S s4;
        processSpan<const S>({s3, s4});
    }

    {
        // const span from non-const span
        auto func2 = [](Span<const int> span) {
        };
        auto func = [&](Span<int> span)
        {
            func2(span);
        };

        std::vector<int> v{1, 2, 3, 4};
        func(v);
        func2(v);
    }

    //  from ptr + size
    {
        std::vector<int> v{1, 2, 3, 4};
        Span s{v.data(), v.size()};
        static_assert(std::is_same_v<decltype(v[0]), decltype(s[0])>);
        abandonIfFalse(v.data() == s.data());
        abandonIfFalse(v.size() == s.size());
        for(int i = 0; i < v.size(); i++)
        {
            abandonIfFalse(v[i] == s[i]);
            v[i] *= 2;
            // pointing to same memory, should still be the same!
            abandonIfFalse(v[i] == s[i]);
        }

        Span<const int> sc{v.data(), v.size()};
        processSpan<const int>({v.data(), v.size()});

        std::vector<int*> vp{&v[1]};
        Span sp{vp.data(), vp.size()};
        static_assert(std::is_same_v<decltype(vp[0]), decltype(sp[0])>);
        abandonIfFalse(vp.data() == sp.data());
    }
    {
        const std::vector<char> v{'1', '2', '3'};
        Span s{v.data(), v.size()};
        static_assert(std::is_same_v<decltype(v[0]), decltype(s[0])>);
        processSpan<const char>({v.data(), v.size()});

        std::vector<const char*> vp{&v[1]};
        Span sp{vp.data(), vp.size()};
        static_assert(std::is_same_v<decltype(vp[0]), decltype(sp[0])>);
        abandonIfFalse(vp.data() == sp.data());
        processSpan<const char*>({vp.data(), vp.size()});

        std::vector<int> vec2{1, 2, 3};
        Span<int> s2{vec2};

        std::vector<int> vec3{1, 2, 3};
        Span<const int> s3{vec3};

        const std::vector<int> vec4{1, 2, 3};
        Span<const int> s4{vec4};
    }

    //  from ptr + ptr
    {
        std::vector<double> v{1, 2, 3, 4};
        Span s{&v[0], &v[3]};
        assert(s.size() == 4);
        static_assert(std::is_same_v<decltype(v[0]), decltype(s[0])>);
        abandonIfFalse(v.data() == s.data());
        abandonIfFalse(v.size() == s.size());
        for(int i = 0; i < v.size(); i++)
        {
            abandonIfFalse(v[i] == s[i]);
            v[i] *= 2;
            // pointing to same memory, should still be the same!
            abandonIfFalse(v[i] == s[i]);
        }

        Span<const double> sc{&v[0], &v[3]};
        processSpan<const double>({v.data(), v.size()});

        std::vector<double*> vp{&v[1]};
        Span sp{&vp[0], &vp[0]};
        static_assert(std::is_same_v<decltype(vp[0]), decltype(sp[0])>);
        abandonIfFalse(vp.data() == sp.data());
    }

    //  from vector
    {
        std::vector<std::string> v{"1", "2", "3", "4"};
        Span<std::string> s{v};
        static_assert(std::is_same_v<decltype(v[0]), decltype(s[0])>);
        abandonIfFalse(v.data() == s.data());
        static_assert(std::is_same_v<decltype(v.data()), decltype(s.data())>);
        abandonIfFalse(v.size() == s.size());
        for(int i = 0; i < v.size(); i++)
        {
            abandonIfFalse(v[i] == s[i]);
            v[i] += "2";
            // pointing to same memory, should still be the same!
            abandonIfFalse(v[i] == s[i]);
            s[0] += "3";
            // pointing to same memory, should still be the same!
            abandonIfFalse(v[i] == s[i]);
        }
        Span<std::string> s2{v};
        Span<std::string> s3{v};
        Span<std::string> s4 = v;
        processSpan<std::string>(v);
        Span<const std::string> s5{v};
        processSpan<const std::string>(v);
    }
    {
        const std::vector<int> v{1, 2, 3};
        static_assert(std::is_const<decltype(v)>::value);
        Span<const int> s{v};

        std::vector<int> v2{1, 2, 3};
        Span<const int> s2{v2};
        Span<int> s22{v2};
    }

    // from std::array
    {
        std::array<int, 4> v{1, 2, 3, 4};
        Span<const int> s1{v};
        Span<int> s{v};
        static_assert(std::is_same_v<decltype(v[0]), decltype(s[0])>);
        abandonIfFalse(v.data() == s.data());
        static_assert(std::is_same_v<decltype(v.data()), decltype(s.data())>);
        abandonIfFalse(v.size() == s.size());
        for(int i = 0; i < v.size(); i++)
        {
            abandonIfFalse(v[i] == s[i]);
            v[i] *= 2;
            // pointing to same memory, should still be the same!
            abandonIfFalse(v[i] == s[i]);
            s[0] -= 3;
            // pointing to same memory, should still be the same!
            abandonIfFalse(v[i] == s[i]);
        }
    }
    {
        std::array<const int, 3> v{1, 2, 3};
        Span<const int> s{v};
        static_assert(std::is_same_v<decltype(v[0]), decltype(s[0])>);
        abandonIfFalse(v.data() == s.data());
        static_assert(std::is_same_v<decltype(v.data()), decltype(s.data())>);
    }

    // from c array
    {
        float a[4] = {1.f, 2.f, 3.f, 4.f};
        Span s{a, 4};
        static_assert(std::is_same_v<decltype(a[0]), decltype(s[0])>);
        abandonIfFalse(&a[3] == &s[3]);
        a[2] = -1.23f;
        abandonIfFalse(a[2] == s[2]);
    }
    {
        const float a[4] = {1.f, 2.f, 3.f, 4.f};
        Span s{a, 4};
        static_assert(std::is_same_v<decltype(a[0]), decltype(s[0])>);
        abandonIfFalse(&a[3] == &s[3]);
    }

    return 0;
}