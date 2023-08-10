/*
    based on:
        https://stackoverflow.com/a/32593825
*/

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <type_traits>
#include <vector>

class ThreadPool
{
  public:
    void start(uint32_t numThreads = std::thread::hardware_concurrency());

    // based on https://www.cnblogs.com/sinkinben/p/16064857.html#:~:text=.-,enqueue,-Recall%20that%20we
    template <typename F, typename... Args>
        requires std::is_invocable_v<F, int, Args...>
    std::future<std::invoke_result_t<F, int, Args...>> queueJob(F&& func, Args&&... args)
    {
        using returnType = std::invoke_result_t<F, int, Args...>;

        // wrap in smart pointer to ensure lifetime outside of this function
        auto task = std::make_shared<std::packaged_task<returnType(int)>>(
            std::bind(std::forward<F>(func), std::placeholders::_1, std::forward<Args>(args)...));
        // placeholder is so first argument (threadIndex) does not get bound

        std::future<returnType> future = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            jobs.push([task](int i) { (*task)(i); });
        }

        mutexCondition.notify_one();
        return future;
    }

    bool busy();
    void stop();

    inline uint32_t amountOfThreads() const
    {
        return threads.size();
    }

    int getThreadPoolThreadIndex();

  private:
    void threadLoop();

    bool shouldTerminate = false;           // If set threads will stop looking for job
    std::mutex queueMutex;                  // Prevent race condition when accessing job queue
    std::condition_variable mutexCondition; // Allows threads to wait on new jobs or termination
    std::vector<std::thread> threads;
    std::queue<std::function<void(int)>> jobs;

    std::atomic<int> freeThreadIndex = 0;
};