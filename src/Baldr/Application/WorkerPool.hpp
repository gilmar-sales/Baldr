/**
 * @file Application/WorkerPool.hpp
 * @brief Fixed-size thread pool used to dispatch background work such as
 *        asynchronous completion of streaming responses.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Simple @c std::thread-backed worker pool.
     *
     * Constructed with a fixed thread count (defaulting to
     * @c std::thread::hardware_concurrency); tasks submitted via @ref Submit
     * are queued FIFO and run on the next available worker. Exceptions thrown
     * by submitted callables surface through the returned @c std::future.
     *
     * Not copyable. The destructor signals workers to stop and joins them,
     * dropping any tasks that had not yet been picked up.
     */
    class WorkerPool
    {
      public:
        /**
         * @brief Spawn @p threadCount worker threads.
         *
         * @param threadCount Number of workers. When 0, defaults to
         *        @c std::max(1, std::thread::hardware_concurrency()).
         */
        explicit WorkerPool(std::size_t threadCount = 0)
        {
            if (threadCount == 0)
            {
                threadCount =
                    std::max<std::size_t>(1,
                                          std::thread::hardware_concurrency());
            }

            for (std::size_t i = 0; i < threadCount; ++i)
            {
                mWorkers.emplace_back([this]() { workerLoop(); });
            }
        }

        /**
         * @brief Signal all workers to stop and join them.
         *
         * Tasks remaining in the queue at this point are dropped.
         */
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

        /**
         * @brief Enqueue @p f for asynchronous execution.
         *
         * @tparam F Callable type.
         * @param f  Callable to invoke on a worker thread.
         * @return A @c std::future that resolves to the callable's return value
         *         (or rethrows its exception).
         * @throws std::runtime_error If the pool has been stopped.
         */
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
        /**
         * @brief Per-worker entry point: block on @c mCondition, pop the
         *        next task and run it. Exits when @c mStop is set and the
         *        queue is drained.
         */
        void workerLoop()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mMutex);
                    mCondition.wait(lock, [this]() {
                        return mStop || !mTasks.empty();
                    });
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

} // namespace BALDR_NAMESPACE