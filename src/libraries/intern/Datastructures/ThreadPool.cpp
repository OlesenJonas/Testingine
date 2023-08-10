#include "ThreadPool.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <mutex>

void ThreadPool::start(uint32_t numThreads)
{
    assert(numThreads > 0);
    threads.resize(numThreads);
    for(uint32_t i = 0; i < numThreads; i++)
    {
        threads.at(i) = std::thread(&ThreadPool::threadLoop, this);
    }
}

void ThreadPool::threadLoop()
{
    int index = getThreadPoolThreadIndex();
    while(true)
    {
        std::function<void(int)> job;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            mutexCondition.wait(lock, [this] { return !jobs.empty() || shouldTerminate; });
            if(shouldTerminate)
            {
                return;
            }
            job = jobs.front();
            jobs.pop();
        }
        job(index);
    }
    assert(false && "Should not be reached!");
}

bool ThreadPool::busy()
{
    bool poolbusy;
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        poolbusy = !jobs.empty();
    }
    return poolbusy;
}

void ThreadPool::stop()
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        shouldTerminate = true;
    }
    mutexCondition.notify_all();
    for(std::thread& activeThread : threads)
    {
        activeThread.join();
    }
    threads.clear();
}

int ThreadPool::getThreadPoolThreadIndex()
{
    const thread_local int myIndex = freeThreadIndex.fetch_add(1);
    return myIndex;
}