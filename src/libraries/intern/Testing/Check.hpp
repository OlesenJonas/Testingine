#pragma once

#include <type_traits>

struct Check
{
    template <typename T1, typename T2>
        requires std::is_convertible<T2, T1>::value
    static bool Equal(T1 left, T2 right, const char* file = "", int line = -1)
    {
        if(left != right)
        {
            printf("Equal check failed: %s line %d \n", file, line);
            return false;
        }
        return true;
    }

    template <typename T1, typename T2>
        requires std::is_convertible<T2, T1>::value
    static bool NotEqual(T1 left, T2 right, const char* file = "", int line = -1)
    {
        if(left == right)
        {
            // very useful if testing is done with debug builds
            printf("NotEqual check failed: %s line %d \n", file, line);
            return false;
        }
        return true;
    }
};

#define CheckEqual(x, y) Check::Equal(x, y, __FILE__, __LINE__)
#define CheckNotEqual(x, y) Check::NotEqual(x, y, __FILE__, __LINE__)