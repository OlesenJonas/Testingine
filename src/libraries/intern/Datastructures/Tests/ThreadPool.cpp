#include <Datastructures/ThreadPool.hpp>

#include <chrono>
#include <iostream>
#include <unordered_map>

int main()
{
    using namespace std::chrono_literals;

    ThreadPool pool;
    pool.start(3);

    auto future1 = pool.queueJob(
        [&](int threadIndex)
        {
            std::this_thread::sleep_for(1500ms);
            std::cout << "Hello from thread with index " << threadIndex << std::endl;
        });

    auto future2 = pool.queueJob(
        [&](int threadIndex)
        {
            std::this_thread::sleep_for(1000ms);
            std::cout << "Hello from thread with index " << threadIndex << std::endl;
        });

    auto future3 = pool.queueJob(
        [&](int threadIndex)
        {
            std::this_thread::sleep_for(2000ms);
            std::cout << "Hello from thread with index " << threadIndex << std::endl;
        });

    future3.wait();
    future2.wait();
    future1.wait();

    auto someTask = [](int threadIndex, int num)
    {
        std::this_thread::sleep_for(1000ms);
        int res = num * 2;
        std::cout << "Calculation result: " << res << std::endl;
        return res;
    };

    auto future4 = pool.queueJob(someTask, 13);
    int res = future4.get();
    std::cout << "From future: " << res << std::endl;

    while(pool.busy())
        ;
    pool.stop();
}