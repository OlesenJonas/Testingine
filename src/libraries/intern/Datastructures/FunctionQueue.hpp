#pragma once

#include <functional>

#include <deque>

// Doesnt handle any parameters for now

// template <std::invocable Function = std::function<void()>>
template <typename Function = std::function<void()>>
    requires std::invocable<Function&>
struct FunctionQueue
{
    std::deque<Function> functions;

    void pushBack(Function&& function)
    {
        functions.push_back(function);
    }

    void flushReverse()
    {
        // reverse iterate the deletion queue to execute all the functions
        for(auto it = functions.rbegin(); it != functions.rend(); it++)
        {
            (*it)(); // call the function
        }
        functions.clear();
    }
};