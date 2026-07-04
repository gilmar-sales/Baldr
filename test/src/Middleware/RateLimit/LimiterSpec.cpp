#include <Baldr/Middleware/RateLimit/Limiter.hpp>

#include <atomic>
#include <thread>
#include <unordered_set>
#include <vector>

class RateLimiterSpec : public ::testing::Test
{
};

// ============================================================================
// CWE-770: Rate Limiting
// ============================================================================
TEST_F(RateLimiterSpec, RateLimiterShouldBlockExcessiveRequests)
{
    baldr::RateLimiter limiter(/*maxRequests=*/3, std::chrono::seconds(10));

    // First 3 requests should be allowed
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));

    // 4th request should be blocked
    EXPECT_FALSE(limiter.isAllowed("192.168.1.1"));
}

TEST_F(RateLimiterSpec, RateLimiterShouldAllowDifferentClients)
{
    baldr::RateLimiter limiter(/*maxRequests=*/3, std::chrono::seconds(10));

    // Different IPs should have independent rate limits
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.1"));

    EXPECT_TRUE(limiter.isAllowed("192.168.1.2"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.2"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.2"));

    EXPECT_TRUE(limiter.isAllowed("192.168.1.3"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.3"));
    EXPECT_TRUE(limiter.isAllowed("192.168.1.3"));
}

TEST_F(RateLimiterSpec, RealRateLimiterAllowsUpToCapacity)
{
    baldr::RateLimiter limiter(/*maxRequests=*/3, std::chrono::seconds(10));

    EXPECT_TRUE(limiter.isAllowed("client-a"));
    EXPECT_TRUE(limiter.isAllowed("client-a"));
    EXPECT_TRUE(limiter.isAllowed("client-a"));
    EXPECT_FALSE(limiter.isAllowed("client-a"));
}

TEST_F(RateLimiterSpec, RealRateLimiterRefillsTokensAfterWindow)
{
    // 3 requests per 100 ms.
    baldr::RateLimiter limiter(
        /*maxRequests=*/3, std::chrono::milliseconds(100));

    EXPECT_TRUE(limiter.isAllowed("client-b"));
    EXPECT_TRUE(limiter.isAllowed("client-b"));
    EXPECT_TRUE(limiter.isAllowed("client-b"));
    EXPECT_FALSE(limiter.isAllowed("client-b"));

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Window has elapsed: tokens fully refilled.
    EXPECT_TRUE(limiter.isAllowed("client-b"));
    EXPECT_TRUE(limiter.isAllowed("client-b"));
    EXPECT_TRUE(limiter.isAllowed("client-b"));
    EXPECT_FALSE(limiter.isAllowed("client-b"));
}

TEST_F(RateLimiterSpec, RealRateLimiterIsThreadSafe)
{
    // Two threads hammering the same client must not race past the limit.
    baldr::RateLimiter limiter(/*maxRequests=*/100, std::chrono::seconds(60));

    std::atomic<int> allowedCount { 0 };
    auto             worker = [&]() {
        for (int i = 0; i < 200; ++i)
        {
            if (limiter.isAllowed("client-c"))
                allowedCount.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();

    // Total attempts: 400. Capacity: 100. Exactly 100 must have succeeded.
    EXPECT_EQ(allowedCount.load(), 100);
}

TEST_F(RateLimiterSpec, RealRateLimiterEvictsLeastRecentlyUsed)
{
    // Capacity 2 means only 2 distinct clients can be tracked before LRU
    // eviction kicks in.
    baldr::RateLimiter limiter(/*maxRequests=*/5, std::chrono::seconds(60),
                               /*maxTrackedClients=*/2);

    EXPECT_TRUE(limiter.isAllowed("client-x"));
    EXPECT_TRUE(limiter.isAllowed("client-y"));

    // Inserting "client-z" should evict "client-x" (least recently used).
    EXPECT_TRUE(limiter.isAllowed("client-z"));

    EXPECT_TRUE(limiter.isAllowed("client-y"));
    EXPECT_TRUE(limiter.isAllowed("client-x"));
}
