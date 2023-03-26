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
    }

    //  from ptr + ptr
    {
        std::vector<double> v{1, 2, 3, 4};
        Span s{&v[0], &v[3]};
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
        auto s = makeSpan(v);
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
        Span s2{v};
        Span<std::string> s3{v};
        Span<std::string> s4 = v;
        processSpan<std::string>(v);
        Span<const std::string> s5{v};
        processSpan<const std::string>(v);
        Span<const std::string> cs = makeConstSpan(v);
        processSpan<const std::string>(makeConstSpan(v));

        processSpan(makeSpan(v));
        processSpan<std::string>(makeSpan(v));
        // todo: this should work aswell!
        // processSpan<const int>(makeSpan(v));

        std::vector<std::string*> vp{&v[0], &v[1]};
        auto s6 = makeSpan(vp);
    }
    {
        const std::vector<int> v{1, 2, 3};
        static_assert(std::is_const<decltype(v)>::value);
        auto s = makeSpan(v);
        static_assert(std::is_same_v<decltype(v[0]), decltype(s[0])>);
        abandonIfFalse(v.data() == s.data());
        static_assert(std::is_same_v<decltype(v.data()), decltype(s.data())>);
        processSpan<const int>(makeSpan(v));
    }

    // from std::array
    {
        std::array<int, 4> v{1, 2, 3, 4};
        auto s = makeSpan(v);
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
        const std::array<int, 3> v{1, 2, 3};
        auto s = makeSpan(v);
        static_assert(std::is_same_v<decltype(v[0]), decltype(s[0])>);
        abandonIfFalse(v.data() == s.data());
        static_assert(std::is_same_v<decltype(v.data()), decltype(s.data())>);
    }
    {
        std::array<const int, 3> v{1, 2, 3};
        // Span<const int> s{v};
        auto s = makeSpan(v);
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