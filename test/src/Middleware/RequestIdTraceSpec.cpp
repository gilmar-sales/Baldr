#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/StatusCode.hpp>
#include <Baldr/Middleware/RequestId.hpp>

#include <gtest/gtest.h>

class RequestIdTraceSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method  = baldr::HttpMethod::Get;
        mRequest.path    = "/";
        mRequest.version = "HTTP/1.1";
        mResponse        = baldr::HttpResponse(mRequest);
    }

    baldr::HttpRequest  mRequest;
    baldr::HttpResponse mResponse;
};

TEST_F(RequestIdTraceSpec, GeneratesTraceContextWhenAbsent)
{
    baldr::RequestIdMiddleware mw;
    mw.Handle(mRequest, mResponse, []() {});

    ASSERT_TRUE(mRequest.traceContext.valid);
    EXPECT_EQ(mRequest.traceContext.traceId.size(), 32u);
    EXPECT_EQ(mRequest.traceContext.spanId.size(), 16u);
    EXPECT_FALSE(mRequest.traceContext.traceId.empty());

    EXPECT_FALSE(mResponse.headers.at("X-Request-ID").empty());
    EXPECT_EQ(mResponse.headers.at("X-Request-ID"),
              mRequest.traceContext.traceId);
    ASSERT_NE(mResponse.headers.find("traceparent"), mResponse.headers.end());
}

TEST_F(RequestIdTraceSpec, ParsesIncomingTraceparentAndMintsNewSpan)
{
    baldr::RequestIdMiddleware mw;
    const std::string incomingTraceId = "0af7651916cd43dd8448eb211c80319c";
    const std::string incomingSpanId  = "b7ad6b7169203331";
    mRequest.headers["traceparent"] =
        "00-" + incomingTraceId + "-" + incomingSpanId + "-01";

    mw.Handle(mRequest, mResponse, []() {});

    ASSERT_TRUE(mRequest.traceContext.valid);
    EXPECT_EQ(mRequest.traceContext.traceId, incomingTraceId);
    EXPECT_NE(mRequest.traceContext.spanId, incomingSpanId);
    EXPECT_EQ(mRequest.traceContext.traceFlags, 0x01);
    EXPECT_TRUE(mRequest.traceContext.sampled());

    const auto& outbound = mResponse.headers.at("traceparent");
    EXPECT_NE(outbound.find(incomingTraceId), std::string::npos);
    EXPECT_EQ(outbound.find(incomingSpanId), std::string::npos);
    EXPECT_EQ(mResponse.headers.at("X-Request-ID"), incomingTraceId);
}

TEST_F(RequestIdTraceSpec, PreservesExplicitClientRequestId)
{
    baldr::RequestIdMiddleware mw;
    mRequest.headers["x-request-id"] = "client-supplied";
    mRequest.headers["traceparent"] =
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01";

    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.at("X-Request-ID"), "client-supplied");
    EXPECT_EQ(mRequest.headers.at("X-Request-ID"), "client-supplied");
}

TEST_F(RequestIdTraceSpec, MalformedTraceparentFallsBackToGeneration)
{
    baldr::RequestIdMiddleware mw;
    mRequest.headers["traceparent"] = "not-a-traceparent";

    mw.Handle(mRequest, mResponse, []() {});

    ASSERT_TRUE(mRequest.traceContext.valid);
    EXPECT_EQ(mRequest.traceContext.traceId.size(), 32u);
    EXPECT_EQ(mRequest.traceContext.spanId.size(), 16u);
    EXPECT_FALSE(mResponse.headers.at("X-Request-ID").empty());
}

TEST_F(RequestIdTraceSpec, AllZeroTraceIdIsRegenerated)
{
    baldr::RequestIdMiddleware mw;
    mRequest.headers["traceparent"] =
        "00-00000000000000000000000000000000-b7ad6b7169203331-01";

    mw.Handle(mRequest, mResponse, []() {});

    ASSERT_TRUE(mRequest.traceContext.valid);
    EXPECT_NE(mRequest.traceContext.traceId,
              "00000000000000000000000000000000");
    EXPECT_NE(mRequest.traceContext.spanId, "b7ad6b7169203331");
}

TEST_F(RequestIdTraceSpec, DisablingResponsePropagationOmitsTraceparent)
{
    baldr::RequestIdOptions opts;
    opts.propagateTraceparentResponse = false;
    baldr::RequestIdMiddleware mw(opts);

    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.find("traceparent"), mResponse.headers.end());
    EXPECT_FALSE(mResponse.headers.at("X-Request-ID").empty());
    ASSERT_TRUE(mRequest.traceContext.valid);
}

TEST_F(RequestIdTraceSpec, DisablingRequestIdFallbackUsesGeneratedId)
{
    baldr::RequestIdOptions opts;
    opts.useTraceIdAsRequestIdFallback = false;
    baldr::RequestIdMiddleware mw(opts);

    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_FALSE(mResponse.headers.at("X-Request-ID").empty());
    EXPECT_NE(mResponse.headers.at("X-Request-ID"),
              mRequest.traceContext.traceId);
}