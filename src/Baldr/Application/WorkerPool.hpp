#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class WorkerPool
{
  public:
    explicit WorkerPool(std::size_t threadCount = 0)
    {
        if (threadCount == 0)
        {
            threadCount =
                std::max<std::size_t>(1, std::thread::hardware_concurrency());
        }

        for (std::size_t i = 0; i < threadCount; ++i)
        {
            mWorkers.emplace_back([this]() { workerLoop(); });
        }
    }

    ~WorkerPool()
    {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStop = true;
        }
        mCondition.notify_all();
        for (auto& worker : mWorkers)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    WorkerPool(const WorkerPool&)            = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    template <typename F>
    auto Submit(F&& f) -> std::future<std::invoke_result_t<F>>
    {
        using ReturnType = std::invoke_result_t<F>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::forward<F>(f));
        auto future = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mStop)
                throw std::runtime_error("WorkerPool is stopped");
            mTasks.emplace([task]() { (*task)(); });
        }
        mCondition.notify_one();
        return future;
    }

  private:
    void workerLoop()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mCondition.wait(lock,
                                [this]() { return mStop || !mTasks.empty(); });
                if (mStop && mTasks.empty())
                    return;
                task = std::move(mTasks.front());
                mTasks.pop();
            }
            task();
        }
    }

    std::vector<std::thread>          mWorkers;
    std::queue<std::function<void()>> mTasks;
    std::mutex                        mMutex;
    std::condition_variable           mCondition;
    bool                              mStop = false;
};