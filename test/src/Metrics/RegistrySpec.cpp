#include <Baldr/Metrics/Registry.hpp>

#include <memory>
#include <string>

class MetricsRegistrySpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Test-only ctor to bypass the singleton.
        mReg = std::make_unique<baldr::MetricsRegistry>(
            baldr::MetricsRegistry::TestOnlyTag {});
    }

    std::unique_ptr<baldr::MetricsRegistry> mReg;
};

TEST_F(MetricsRegistrySpec, IncrementsRequestCounts)
{
    mReg->incRequest("GET", 200);
    mReg->incRequest("GET", 200);
    mReg->incRequest("GET", 404);
    EXPECT_EQ(mReg->requestCount(), 3u);
}

TEST_F(MetricsRegistrySpec, ObservesLatencyBuckets)
{
    mReg->observeLatencySeconds("GET", "/", 0.003);
    mReg->observeLatencySeconds("GET", "/", 0.04);
    mReg->observeLatencySeconds("GET", "/", 0.6);
    auto body = mReg->renderPrometheus();
    EXPECT_NE(body.find("baldr_http_request_duration_seconds_bucket"),
              std::string::npos);
    EXPECT_NE(body.find("baldr_http_request_duration_seconds_count"),
              std::string::npos);
}

TEST_F(MetricsRegistrySpec, TracksInFlight)
{
    mReg->incInFlight(+1);
    mReg->incInFlight(+1);
    mReg->incInFlight(-1);
    EXPECT_EQ(mReg->inFlight(), 1);
}

TEST_F(MetricsRegistrySpec, RendersPrometheusExposition)
{
    mReg->incRequest("GET", 200);
    mReg->incRequest("POST", 201);
    mReg->observeLatencySeconds("GET", "/", 0.012);
    auto text = mReg->renderPrometheus();
    EXPECT_NE(text.find("baldr_http_requests_total"), std::string::npos)
        << text;
    EXPECT_NE(text.find("status=\"200\""), std::string::npos) << text;
    EXPECT_NE(text.find("method=\"POST\""), std::string::npos) << text;
    EXPECT_NE(text.find("baldr_http_in_flight_requests"), std::string::npos)
        << text;
}
