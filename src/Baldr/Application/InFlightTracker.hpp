/**
 * @file Application/InFlightTracker.hpp
 * @brief Process-wide counter of in-flight HTTP handlers, used by
 *        @c HttpServer to perform a graceful drain on shutdown.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Counts HTTP handlers currently executing across all connections.
     *
     * Each handler call is bracketed by @ref enter / @ref leave. On shutdown,
     * the server calls @ref waitDrained to give in-flight requests a chance
     * to complete before forcibly closing sockets.
     */
    class InFlightTracker
    {
      public:
        /**
         * @brief Increment the in-flight counter. Called before a handler runs.
         */
        void enter() { mCount.fetch_add(1, std::memory_order_acq_rel); }

        /**
         * @brief Decrement the in-flight counter. Called after a handler
         *        returns (including via exception). If the counter reaches
         *        zero, any thread blocked in @ref waitDrained is notified.
         */
        void leave()
        {
            if (mCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
                mCv.notify_all();
        }

        /**
         * @brief Number of handlers currently in flight.
         */
        std::size_t outstanding() const
        {
            return mCount.load(std::memory_order_acquire);
        }

        /**
         * @brief Block until @ref outstanding reaches zero or @p timeout
         *        elapses, whichever comes first.
         *
         * @param timeout Maximum time to wait.
         */
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
        std::condition_variable  mCv;
        std::mutex               mMutex;
    };

} // namespace BALDR_NAMESPACE
