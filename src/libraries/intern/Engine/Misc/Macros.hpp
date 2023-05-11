#pragma once

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <debugapi.h>

    #define BREAKPOINT                                                                                            \
        {                                                                                                         \
            if(IsDebuggerPresent())                                                                               \
                __debugbreak();                                                                                   \
        }
#else
    // not sure this is correct, cant test it
    // #define BREAKPOINT __builtin_trap()
    #define BREAKPOINT
#endif

#define CREATE_STATIC_GETTER(T)                                                                                   \
  private:                                                                                                        \
    inline static T* ptr = nullptr;                                                                               \
                                                                                                                  \
  public:                                                                                                         \
    [[nodiscard]] static inline T* get()                                                                          \
    {                                                                                                             \
        assert(ptr != nullptr && "Static pointer for type " #T "was not initialized!");                           \
        return ptr;                                                                                               \
    }