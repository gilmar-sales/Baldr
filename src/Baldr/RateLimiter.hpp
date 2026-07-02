#pragma once

#include <algorithm>
#include <chrono>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

class RateLimiter
{
  public:
    RateLimiter(size_t maxRequests, std::chrono::seconds timeWindow,
                size_t maxTrackedClients = 10000) :
        mMaxRequests(maxRequests), mTimeWindow(timeWindow),
        mMaxTrackedClients(maxTrackedClients)
    {
    }

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
            mLruOrder.splice(mLruOrder.begin(), mLruOrder, it->second.lruIt);
        }

        auto& data    = it->second;
        auto& tokens  = data.tokens;
        auto& lastTs  = data.lastTimestamp;

        const auto elapsedMs = std::chrono::duration_cast<
            std::chrono::milliseconds>(now - lastTs).count();
        const auto windowMs = std::chrono::duration_cast<
            std::chrono::milliseconds>(mTimeWindow).count();

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
        std::list<std::string>::iterator      lruIt {};
    };

    size_t                                      mMaxRequests;
    std::chrono::seconds                        mTimeWindow {};
    size_t                                      mMaxTrackedClients;
    std::unordered_map<std::string, ClientData> mClientsData;
    std::list<std::string>                      mLruOrder;
    std::mutex                                  mMutex {};
};