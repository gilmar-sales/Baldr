#include "Baldr/InFlightTracker.hpp"

#include <chrono>
#include <memory>
#include <thread>

class InFlightTrackerSpec : public ::testing::Test
{
  protected:
    InFlightTracker                     mTracker;
};

TEST_F(InFlightTrackerSpec, ZeroByDefault)
{
    EXPECT_EQ(mTracker.outstanding(), 0u);
}

TEST_F(InFlightTrackerSpec, CountsEnterLeave)
{
    mTracker.enter();
    mTracker.enter();
    mTracker.enter();
    mTracker.leave();
    mTracker.leave();
    EXPECT_EQ(mTracker.outstanding(), 1u);
    mTracker.leave();
    EXPECT_EQ(mTracker.outstanding(), 0u);
}

TEST_F(InFlightTrackerSpec, WaitDrainedReturnsWhenZero)
{
    mTracker.enter();
    mTracker.enter();

    std::thread t { [this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        mTracker.leave();
        mTracker.leave();
    } };

    auto before = std::chrono::steady_clock::now();
    mTracker.waitDrained(std::chrono::seconds(1));
    auto after = std::chrono::steady_clock::now();

    t.join();
    EXPECT_EQ(mTracker.outstanding(), 0u);
    EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(
                  after - before).count(),
              500);
}

TEST_F(InFlightTrackerSpec, WaitDrainedTimesOut)
{
    mTracker.enter();
    auto before = std::chrono::steady_clock::now();
    mTracker.waitDrained(std::chrono::milliseconds(50));
    auto after = std::chrono::steady_clock::now();
    EXPECT_EQ(mTracker.outstanding(), 1u);
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(
                  after - before).count(),
              50);
}
