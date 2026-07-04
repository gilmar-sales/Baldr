/**
 * @file Middleware/RateLimit/Limiter.hpp
 * @brief In-memory token-bucket rate limiter with LRU eviction of idle
 *        clients.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <algorithm>
#include <chrono>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Per-client token-bucket rate limiter.
     *
     * Refills @c maxRequests tokens per @c timeWindow, linearly interpolated
     * by elapsed time. The map of tracked clients is bounded by
     * @c maxTrackedClients; the least-recently-used client is evicted when
     * the cap is exceeded. Safe for concurrent use.
     */
    class RateLimiter
    {
      public:
        /**
         * @brief Construct a limiter with the given capacity and window.
         *
         * @tparam Rep    Integer type of the window duration.
         * @tparam Period @c std::ratio of the window duration.
         * @param maxRequests       Maximum requests allowed per window.
         * @param timeWindow        Length of the sliding window.
         * @param maxTrackedClients Maximum distinct clients kept in memory
         *                          before LRU eviction kicks in.
         */
        template <typename Rep, typename Period>
        RateLimiter(size_t maxRequests,
                    std::chrono::duration<Rep, Period>
                           timeWindow,
                    size_t maxTrackedClients = 10000) :
            mMaxRequests(maxRequests),
            mTimeWindow(std::chrono::duration_cast<std::chrono::milliseconds>(
                timeWindow)),
            mMaxTrackedClients(maxTrackedClients)
        {
        }

        /**
         * @brief Check whether @p clientId is allowed to make one more
         *        request right now.
         *
         * Updates the client's token bucket and LRU position atomically.
         *
         * @param clientId Opaque identifier (typically the remote IP).
         * @return @c true when the request is within the rate budget.
         */
        bool isAllowed(const std::string& clientId)
        {
            const auto now = std::chrono::steady_clock::now();

            std::lock_guard lock(mMutex);

            auto [it, inserted] = mClientsData.try_emplace(clientId);
            if (inserted)
            {
                it->second.tokens        = mMaxRequests;
                it->second.lastTimestamp = now;
                mLruOrder.push_front(clientId);
                it->second.lruIt = mLruOrder.begin();

                if (mClientsData.size() > mMaxTrackedClients)
                {
                    const auto& evictedId = mLruOrder.back();
                    mClientsData.erase(evictedId);
                    mLruOrder.pop_back();
                }
            }
            else
            {
                // Promote to front of LRU.
                mLruOrder.splice(mLruOrder.begin(),
                                 mLruOrder,
                                 it->second.lruIt);
            }

            auto& data   = it->second;
            auto& tokens = data.tokens;
            auto& lastTs = data.lastTimestamp;

            const auto elapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - lastTs)
                    .count();
            const auto windowMs = mTimeWindow.count();

            if (windowMs > 0)
            {
                const auto tokensToAdd =
                    static_cast<size_t>(elapsedMs) * mMaxRequests /
                    static_cast<size_t>(windowMs);
                tokens = std::min(mMaxRequests, tokens + tokensToAdd);
            }

            if (now - lastTs > mTimeWindow)
            {
                lastTs = now;
                tokens = mMaxRequests;
            }

            if (tokens >= 1)
            {
                tokens -= 1;
                return true;
            }

            return false;
        }

      private:
        struct ClientData
        {
            size_t                                tokens = 0;
            std::chrono::steady_clock::time_point lastTimestamp =
                std::chrono::steady_clock::now();
            std::list<std::string>::iterator lruIt {};
        };

        size_t                                      mMaxRequests;
        std::chrono::milliseconds                   mTimeWindow {};
        size_t                                      mMaxTrackedClients;
        std::unordered_map<std::string, ClientData> mClientsData;
        std::list<std::string>                      mLruOrder;
        std::mutex                                  mMutex {};
    };

} // namespace BALDR_NAMESPACE