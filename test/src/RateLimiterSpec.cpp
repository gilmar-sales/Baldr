class RateLimiterSpec : public ::testing::Test
{
};

// ============================================================================
// CWE-770: Rate Limiting
// ============================================================================
TEST_F(RateLimiterSpec, RateLimiterShouldBlockExcessiveRequests)
{
    class TestRateLimiter
    {
      public:
        TestRateLimiter() : mClientsData {}, mutex_ {} {}

        bool isAllowed(const std::string& clientId)
        {
            const auto now = std::chrono::steady_clock::now();

            if (!mClientsData.contains(clientId))
                mClientsData[clientId] = { 3, now }; // Only 3 requests allowed

            std::lock_guard<std::mutex> lock(mutex_);

            auto& [tokens, lastTimestamp] = mClientsData[clientId];

            // Simulate token consumption
            if (tokens > 0)
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
            std::chrono::steady_clock::time_point lastTimestamp;
        };
        std::unordered_map<std::string, ClientData> mClientsData;
        std::mutex                                  mutex_;
    };

    TestRateLimiter limiter;

    // First 3 requests should be allowed
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));

    // 4th request should be blocked
    EXPECT_FALSE(limiter.isAllowed("192.168.1.1"));
}

TEST_F(RateLimiterSpec, RateLimiterShouldAllowDifferentClients)
{
    class TestRateLimiter
    {
      public:
        TestRateLimiter() : mClientsData {}, mutex_ {} {}

        bool isAllowed(const std::string& clientId)
        {
            const auto now = std::chrono::steady_clock::now();

            if (!mClientsData.contains(clientId))
                mClientsData[clientId] = { 1, now };

            std::lock_guard<std::mutex> lock(mutex_);

            auto& [tokens, lastTimestamp] = mClientsData[clientId];

            if (tokens > 0)
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
            std::chrono::steady_clock::time_point lastTimestamp;
        };
        std::unordered_map<std::string, ClientData> mClientsData;
        std::mutex                                  mutex_;
    };

    TestRateLimiter limiter;

    // Different IPs should have independent rate limits
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.2"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.3"));
}
