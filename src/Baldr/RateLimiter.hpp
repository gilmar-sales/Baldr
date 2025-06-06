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

    // Check if a client is allowed to make a request
    bool isAllowed(const std::string& clientId)
    {
        const auto now = std::chrono::steady_clock::now();

        if (!mClientsData.contains(clientId))
            mClientsData[clientId] = { mMaxRequests, now };

        std::lock_guard lock(mutex_);

        auto& [tokens, lastTimestamp] = mClientsData[clientId];

        // Calculate elapsed time since last request
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastTimestamp);

        // Refill tokens
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

        // Check if a token is available
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
        size_t                                tokens = 0.0; // Tokens available
        std::chrono::steady_clock::time_point lastTimestamp =
            std::chrono::steady_clock::now();
    };

    size_t mMaxRequests; // Maximum requests allowed per timeWindow
    std::chrono::seconds mTimeWindow {}; // Time window for rate limiting
    std::unordered_map<std::string, ClientData>
               mClientsData; // Client state storage
    std::mutex mutex_ {}; // Protect shared state in multithreaded environments
};