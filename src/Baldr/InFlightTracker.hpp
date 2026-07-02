#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>

// Tracks in-flight HTTP handlers across all connections. Used by
// HttpServer for graceful drain during shutdown.
class InFlightTracker
{
  public:
    void enter() { mCount.fetch_add(1, std::memory_order_acq_rel); }

    void leave()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
            mCv.notify_all();
    }

    std::size_t outstanding() const
    {
        return mCount.load(std::memory_order_acquire);
    }

    // Wait for `outstanding()` to reach zero, or `timeout` to elapse.
    void waitDrained(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (mCount.load(std::memory_order_acquire) == 0)
            return;
        mCv.wait_for(lock, timeout, [this]() {
            return mCount.load(std::memory_order_acquire) == 0;
        });
    }

  private:
    std::atomic<std::size_t> mCount { 0 };
    std::mutex               mMutex;
    std::condition_variable  mCv;
};
