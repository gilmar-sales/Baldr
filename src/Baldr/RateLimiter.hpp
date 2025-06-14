#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

class RateLimiter
{
  public:
    RateLimiter(size_t maxRequests, std::chrono::seconds timeWindow) :
        mMaxRequests(maxRequests), mTimeWindow(timeWindow)
    {
    }

    bool isAllowed(const std::string& clientId)
    {
        const auto now = std::chrono::steady_clock::now();

        if (!mClientsData.contains(clientId))
            mClientsData[clientId] = { mMaxRequests, now };

        std::lock_guard lock(mutex_);

        auto& [tokens, lastTimestamp] = mClientsData[clientId];

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastTimestamp);

        const auto tokensToAdd =
            elapsed.count() *
            (mMaxRequests /
             std::chrono::duration_cast<std::chrono::milliseconds>(mTimeWindow)
                 .count());

        tokens = std::min(mMaxRequests, tokens + tokensToAdd);

        if (now - lastTimestamp > mTimeWindow)
        {
            lastTimestamp = now;
            tokens        = mMaxRequests;
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
        size_t                                tokens = 0.0;
        std::chrono::steady_clock::time_point lastTimestamp =
            std::chrono::steady_clock::now();
    };

    size_t                                      mMaxRequests;
    std::chrono::seconds                        mTimeWindow {};
    std::unordered_map<std::string, ClientData> mClientsData;
    std::mutex                                  mutex_ {};
};