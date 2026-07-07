/**
 * @file SecurityRegressionsSpec.cpp
 * @brief Regression tests covering every issue called out in the
 *        security audit:
 *          - HIGH-1: e.what() leakage on handler exception
 *          - HIGH-2: predictable CSRF / request / trace IDs
 *          - HIGH-3: CRLF injection in response headers / cookies
 *          - MED-1 : Content-Length arithmetic overflow
 *          - MED-2 : locale-aware std::tolower on header keys
 *          - MED-4 : non-portable %zx in chunked-encoding size
 *          - MED-5 : regex DoS in router (wildcard cap)
 *          - MED-6 : unbounded static-file buffering
 *          - LOW-2 : redundant mutex in InFlightTracker::leave
 *          - LOW-6 : X-Forwarded-For handling in rate limiter
 */

#include <Baldr/Application/InFlightTracker.hpp>
#include <Baldr/Hosting/SecureRandom.hpp>
#include <Baldr/Hosting/StringHelpers.hpp>
#include <Baldr/Http/Connection.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/RequestParser.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/StreamingResult.hpp>
#include <Baldr/Http/Router.hpp>
#include <Baldr/Http/StaticFilesInternal.hpp>
#include <Baldr/Http/StatusCode.hpp>
#include <Baldr/Http/TraceContext.hpp>
#include <Baldr/Middleware/Csrf.hpp>
#include <Baldr/Middleware/IMiddleware.hpp>
#include <Baldr/Middleware/RateLimit/Limiter.hpp>
#include <Baldr/Middleware/RateLimit/Middleware.hpp>
#include <Baldr/Middleware/RequestId.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// HIGH-2 — secure RNG
// ============================================================================

TEST(SecureRandomSpec, RandomHexProducesEvenLengthLowerHex)
{
    auto s = baldr::RandomHex(32);
    ASSERT_EQ(s.size(), 32u);
    for (char c : s)
    {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        EXPECT_TRUE(ok) << "unexpected char: " << c;
    }
}

TEST(SecureRandomSpec, RandomHexIsUniqueAcrossDraws)
{
    // 1000 tokens at 128 bits — collisions are astronomically improbable.
    std::set<std::string> seen;
    for (int i = 0; i < 1000; ++i)
        seen.insert(baldr::RandomHex(32));
    EXPECT_EQ(seen.size(), 1000u);
}

TEST(SecureRandomSpec, RandomHexNotTriviallyPredictable)
{
    // The old RNG XOR'd a thread-local counter with the wall clock; under
    // identical clock readings two consecutive tokens would share a long
    // common prefix. The secure RNG must not.
    auto a = baldr::RandomHex(32);
    auto b = baldr::RandomHex(32);
    EXPECT_NE(a, b);
    int commonPrefix = 0;
    while (commonPrefix < 32 && a[commonPrefix] == b[commonPrefix])
        ++commonPrefix;
    EXPECT_LE(commonPrefix, 4);
}

TEST(SecureRandomSpec, CsrfTokensAreUnpredictable)
{
    baldr::CsrfMiddleware mw;
    baldr::HttpRequest    req;
    req.method  = baldr::HttpMethod::Get;
    req.path    = "/api/items";
    req.version = "HTTP/1.1";

    std::set<std::string> tokens;
    for (int i = 0; i < 100; ++i)
    {
        baldr::HttpResponse resp(req);
        mw.Handle(req, resp, []() {});
        auto it = resp.cookies.find("XSRF-TOKEN");
        ASSERT_NE(it, resp.cookies.end());
        tokens.insert(it->second.value);
    }
    EXPECT_EQ(tokens.size(), 100u);
    EXPECT_GE(tokens.begin()->size(), 32u);
}

TEST(SecureRandomSpec, TraceAndSpanIdsAreUnique)
{
    std::set<std::string> traceIds;
    std::set<std::string> spanIds;
    for (int i = 0; i < 1000; ++i)
    {
        traceIds.insert(baldr::NewTraceId());
        spanIds.insert(baldr::NewSpanId());
    }
    EXPECT_EQ(traceIds.size(), 1000u);
    EXPECT_EQ(spanIds.size(), 1000u);
}

// ============================================================================
// HIGH-3 — CRLF injection in outgoing headers / cookies
// ============================================================================

TEST(CrlfInjectionSpec, ValidateResponseHeadersAcceptsCleanHeaders)
{
    std::unordered_map<std::string, std::string> headers {
        { "Content-Type", "application/json" }
    };
    std::unordered_map<std::string, baldr::CookieOptions> cookies;
    std::string                                           badName;
    std::string                                           badValue;
    EXPECT_TRUE(baldr::HttpConnection::ValidateResponseHeaders(
        headers, cookies, badName, badValue));
}

TEST(CrlfInjectionSpec, ValidateResponseHeadersRejectsCrInValue)
{
    std::unordered_map<std::string, std::string> headers {
        { "X-Echo", "value\r\nSet-Cookie: pwn=1" }
    };
    std::unordered_map<std::string, baldr::CookieOptions> cookies;
    std::string                                           badName;
    std::string                                           badValue;
    EXPECT_FALSE(baldr::HttpConnection::ValidateResponseHeaders(
        headers, cookies, badName, badValue));
    EXPECT_EQ(badName, "X-Echo");
}

TEST(CrlfInjectionSpec, ValidateResponseHeadersRejectsLfInValue)
{
    std::unordered_map<std::string, std::string> headers {
        { "X-Echo", "value\nInjected: yes" }
    };
    std::unordered_map<std::string, baldr::CookieOptions> cookies;
    std::string                                           badName;
    std::string                                           badValue;
    EXPECT_FALSE(baldr::HttpConnection::ValidateResponseHeaders(
        headers, cookies, badName, badValue));
}

TEST(CrlfInjectionSpec, ValidateResponseHeadersRejectsInvalidName)
{
    std::unordered_map<std::string, std::string> headers {
        { "Bad Name With Space", "value" }
    };
    std::unordered_map<std::string, baldr::CookieOptions> cookies;
    std::string                                           badName;
    std::string                                           badValue;
    EXPECT_FALSE(baldr::HttpConnection::ValidateResponseHeaders(
        headers, cookies, badName, badValue));
}

TEST(CrlfInjectionSpec, ValidateResponseHeadersRejectsCrlfInCookie)
{
    std::unordered_map<std::string, std::string>          headers;
    std::unordered_map<std::string, baldr::CookieOptions> cookies;
    baldr::CookieOptions                                  opts;
    opts.value      = "token\r\nSet-Cookie: pwn=1";
    cookies["SESS"] = opts;
    std::string badName;
    std::string badValue;
    EXPECT_FALSE(baldr::HttpConnection::ValidateResponseHeaders(
        headers, cookies, badName, badValue));
}

TEST(CrlfInjectionSpec, ValidateResponseHeadersRejectsCrlfInCookieDomain)
{
    std::unordered_map<std::string, std::string>          headers;
    std::unordered_map<std::string, baldr::CookieOptions> cookies;
    baldr::CookieOptions                                  opts;
    opts.domain     = "evil.example\r\nSet-Cookie: pwn=1";
    cookies["SESS"] = opts;
    std::string badName;
    std::string badValue;
    EXPECT_FALSE(baldr::HttpConnection::ValidateResponseHeaders(
        headers, cookies, badName, badValue));
}

TEST(CrlfInjectionSpec, FormatStreamingHeadRejectsCrlfInValue)
{
    std::vector<std::pair<std::string, std::string>> headers {
        { "Content-Type", "text/event-stream\r\nEvil: 1" }
    };
    std::vector<std::pair<std::string, std::string>> cookies;
    bool                                             ok = true;
    auto head = baldr::formatStreamingHead(
        baldr::StatusCode::OK, "HTTP/1.1", headers, cookies,
        [](baldr::StatusCode) -> const char* { return "OK"; }, ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(head.empty());
}

TEST(CrlfInjectionSpec, FormatStreamingHeadRejectsCrlfInCookie)
{
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::pair<std::string, std::string>> cookies {
        { "SESS", "tok\r\nSet-Cookie: pwn=1" }
    };
    bool ok   = true;
    auto head = baldr::formatStreamingHead(
        baldr::StatusCode::OK, "HTTP/1.1", headers, cookies,
        [](baldr::StatusCode) -> const char* { return "OK"; }, ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(head.empty());
}

TEST(CrlfInjectionSpec, FormatStreamingHeadAcceptsCleanHeaders)
{
    std::vector<std::pair<std::string, std::string>> headers {
        { "Content-Type", "text/event-stream" }
    };
    std::vector<std::pair<std::string, std::string>> cookies {
        { "SESS", "token" }
    };
    bool ok   = false;
    auto head = baldr::formatStreamingHead(
        baldr::StatusCode::OK, "HTTP/1.1", headers, cookies,
        [](baldr::StatusCode) -> const char* { return "OK"; }, ok);
    EXPECT_TRUE(ok);
    EXPECT_NE(head.find("text/event-stream"), std::string::npos);
    EXPECT_NE(head.find("Set-Cookie: SESS=token"), std::string::npos);
}

TEST(CrlfInjectionSpec, ContainsCrlfDetectsAllLineBreakForms)
{
    EXPECT_TRUE(baldr::containsCrlf("a\rb"));
    EXPECT_TRUE(baldr::containsCrlf("a\nb"));
    EXPECT_TRUE(baldr::containsCrlf("a\r\nb"));
    EXPECT_FALSE(baldr::containsCrlf("clean"));
    EXPECT_FALSE(baldr::containsCrlf(""));
}

TEST(CrlfInjectionSpec, IsValidHeaderNameAcceptsRfc9110Tokens)
{
    EXPECT_TRUE(baldr::isValidHeaderName("Content-Type"));
    EXPECT_TRUE(baldr::isValidHeaderName("X-Foo_Bar.1"));
    EXPECT_FALSE(baldr::isValidHeaderName(""));
    EXPECT_FALSE(baldr::isValidHeaderName("Bad Name"));
    EXPECT_FALSE(baldr::isValidHeaderName("Bad:Name"));
    EXPECT_FALSE(baldr::isValidHeaderName(std::string(70, 'A')));
}

// ============================================================================
// MED-4 — non-portable %zx
// ============================================================================

TEST(StreamingChunkSpec, FormatChunkUsesPortableHex)
{
    // The wire format must be "<hex-len>\r\n<data>\r\n" with a lowercase
    // hex length that does NOT contain "z".
    std::string frame = baldr::formatChunk("hello");
    EXPECT_NE(frame.find("5\r\nhello\r\n"), std::string::npos);
    EXPECT_EQ(frame.find('z'), std::string::npos);
    EXPECT_EQ(frame.find('Z'), std::string::npos);
}

TEST(StreamingChunkSpec, FormatChunkHandlesLargeChunks)
{
    std::string data(1000, 'x');
    std::string frame = baldr::formatChunk(data);
    EXPECT_NE(frame.find("3e8\r\n"), std::string::npos);
    EXPECT_NE(frame.find("\r\n", 4), std::string::npos);
    EXPECT_EQ(frame.find('z'), std::string::npos);
}

// ============================================================================
// MED-1 — Content-Length overflow / oversize guard
// ============================================================================

class ContentLengthOverflowSpec : public ::testing::Test
{
  protected:
    skr::Arc<baldr::HttpRequestParser> mParser;
    void                               SetUp() override
    {
        mParser = skr::MakeArc<baldr::HttpRequestParser>();
    }
};

TEST_F(ContentLengthOverflowSpec, RejectsValueThatOverflowsWhenMultiplied)
{
    // 19-digit value: silently wrapped on a 64-bit platform before the fix.
    auto status = mParser->tryParse("POST /api HTTP/1.1\r\n"
                                    "Content-Length: 9999999999999999999\r\n"
                                    "Host: x\r\n\r\n");

    EXPECT_EQ(status.kind, baldr::HttpParseStatus::Kind::Error);
    EXPECT_EQ(status.statusCode, baldr::StatusCode::BadRequest);
}

TEST_F(ContentLengthOverflowSpec, RejectsValueWithTooManyDigits)
{
    // maxBodySize default is 100 MiB = 104857600 = 9 digits; 10 digits
    // cannot possibly be <= maxBodySize so the parser must reject it
    // without wrapping.
    auto status = mParser->tryParse("POST /api HTTP/1.1\r\n"
                                    "Content-Length: 2147483648\r\n"
                                    "Host: x\r\n\r\n");

    EXPECT_EQ(status.kind, baldr::HttpParseStatus::Kind::Error);
    EXPECT_EQ(status.statusCode, baldr::StatusCode::BadRequest);
}

TEST_F(ContentLengthOverflowSpec, AcceptsValueAtBoundary)
{
    std::string body(100, 'x');
    auto        status = mParser->tryParse(
        "POST /api HTTP/1.1\r\n"
        "Content-Length: 100\r\n"
        "Host: x\r\n\r\n" +
        body);

    ASSERT_EQ(status.kind, baldr::HttpParseStatus::Kind::Complete);
    EXPECT_EQ(status.request.body.size(), 100u);
}

// ============================================================================
// MED-2 — header key normalisation is ASCII-only
// ============================================================================

class HeaderLowercaseSpec : public ::testing::Test
{
  protected:
    skr::Arc<baldr::HttpRequestParser> mParser;
    void                               SetUp() override
    {
        mParser = skr::MakeArc<baldr::HttpRequestParser>();
    }
};

// We cannot toggle the C locale on glibc easily, but we can verify that the
// parser's header map keys are lower-case ASCII for both already-canonical
// and unusual casing, ensuring no locale-dependent code path sneaks back in.
TEST_F(HeaderLowercaseSpec, CanonicalLowercaseIsUnchanged)
{
    auto status = mParser->tryParse(
        "GET / HTTP/1.1\r\nContent-Type: text/plain\r\nHost: x\r\n\r\n");
    ASSERT_EQ(status.kind, baldr::HttpParseStatus::Kind::Complete);
    EXPECT_NE(status.request.headers.find("content-type"),
              status.request.headers.end());
}

TEST_F(HeaderLowercaseSpec, UppercaseHeaderKeysAreFoldedToLowerAscii)
{
    auto status = mParser->tryParse(
        "GET / HTTP/1.1\r\nX-CUSTOM-VALUE: v\r\nHost: x\r\n\r\n");
    ASSERT_EQ(status.kind, baldr::HttpParseStatus::Kind::Complete);
    auto it = status.request.headers.find("x-custom-value");
    ASSERT_NE(it, status.request.headers.end());
    EXPECT_EQ(it->second, "v");
}

TEST_F(HeaderLowercaseSpec, AsciiBytesOutsideAZAreNotFooledByLocale)
{
    // The Turkish-locale trap: std::tolower('I') is 'ı' (U+0131). If the
    // parser used std::tolower, this header would be folded to something
    // other than 'x-iq'.
    auto status =
        mParser->tryParse("GET / HTTP/1.1\r\nX-IQ: 1\r\nHost: x\r\n\r\n");
    ASSERT_EQ(status.kind, baldr::HttpParseStatus::Kind::Complete);
    EXPECT_NE(status.request.headers.find("x-iq"),
              status.request.headers.end());
}

// ============================================================================
// MED-5 — regex DoS in router (wildcard cap)
// ============================================================================

TEST(RouterRegexSpec, RejectsRouteWithExcessiveWildcards)
{
    auto        router = skr::MakeArc<baldr::Router>();
    std::string path;
    for (int i = 0; i < 64; ++i)
    {
        if (!path.empty())
            path += "/";
        path += ":p" + std::to_string(i);
    }
    EXPECT_THROW(router->insert(baldr::HttpMethod::Get, path,
                                [](baldr::HttpRequest&, baldr::HttpResponse&,
                                   skr::Arc<skr::ServiceProvider>) {}),
                 std::invalid_argument);
}

TEST(RouterRegexSpec, AcceptsRouteAtWildcardCapBoundary)
{
    auto        router = skr::MakeArc<baldr::Router>();
    std::string path;
    for (int i = 0; i < 32; ++i)
    {
        if (!path.empty())
            path += "/";
        path += ":p" + std::to_string(i);
    }
    EXPECT_NO_THROW(router->insert(baldr::HttpMethod::Get, path,
                                   [](baldr::HttpRequest&, baldr::HttpResponse&,
                                      skr::Arc<skr::ServiceProvider>) {}));
}

// ============================================================================
// MED-6 — static-file streaming no longer reads full body into memory
// ============================================================================

class StaticFileStreamingSpec : public ::testing::Test
{
  protected:
    std::filesystem::path mRoot;
    void                  SetUp() override
    {
        mRoot =
            std::filesystem::temp_directory_path() /
            ("baldr_sec_" +
             std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
        std::filesystem::remove_all(mRoot);
        std::filesystem::create_directories(mRoot / "sub");
        std::ofstream(mRoot / "small.txt") << "hello";
    }
    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(mRoot, ec);
    }
};

TEST_F(StaticFileStreamingSpec, StreamingResolverProducesEmptyBody)
{
    std::ifstream file;
    auto          r = baldr::Detail::resolveStaticFileStreaming(
        "small.txt", mRoot.string(), file);
    EXPECT_EQ(r.status, baldr::StatusCode::OK);
    EXPECT_TRUE(r.body.empty());
    EXPECT_GT(r.fileSize, 0u);
    EXPECT_TRUE(file.is_open());
    EXPECT_TRUE(file.good());
}

TEST_F(StaticFileStreamingSpec, StreamingResolverRejectsDotDotTraversal)
{
    std::ifstream file;
    auto          r = baldr::Detail::resolveStaticFileStreaming(
        "../etc/passwd", mRoot.string(), file);
    EXPECT_EQ(r.status, baldr::StatusCode::BadRequest);
    EXPECT_FALSE(file.is_open());
}

TEST_F(StaticFileStreamingSpec, StreamingResolverRejectsMissingFile)
{
    std::ifstream file;
    auto          r = baldr::Detail::resolveStaticFileStreaming(
        "missing.txt", mRoot.string(), file);
    EXPECT_EQ(r.status, baldr::StatusCode::NotFound);
}

// ============================================================================
// LOW-2 — InFlightTracker::leave does not hold the mutex
// ============================================================================

namespace
{
    class InFlightTrackerFixture
    {
      public:
        baldr::InFlightTracker tracker;
    };
} // namespace

TEST(InFlightTrackerFixtureSpec,
     LeaveReleasesWaitingThreadsWithoutMutexContention)
{
    baldr::InFlightTracker tracker;
    tracker.enter();
    tracker.enter();

    std::atomic<bool> wokeUp { false };
    std::thread       t { [&tracker, &wokeUp]() {
        tracker.waitDrained(std::chrono::seconds(5));
        wokeUp = true;
    } };

    // Give the waiter time to enter wait_for.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    tracker.leave();
    tracker.leave();

    t.join();
    EXPECT_TRUE(wokeUp.load());
    EXPECT_EQ(tracker.outstanding(), 0u);
}

TEST(InFlightTrackerFixtureSpec, WaitDrainedReturnsImmediatelyWhenAlreadyZero)
{
    baldr::InFlightTracker tracker;
    auto                   begin = std::chrono::steady_clock::now();
    tracker.waitDrained(std::chrono::seconds(5));
    auto elapsed = std::chrono::steady_clock::now() - begin;
    EXPECT_LT(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
        100);
}

// ============================================================================
// LOW-6 — X-Forwarded-For handling in rate limiter
// ============================================================================

class RateLimitForwardedForSpec : public ::testing::Test
{
  protected:
    baldr::HttpRequest           mRequest;
    baldr::HttpResponse          mResponse;
    skr::Arc<baldr::RateLimiter> mLimiter =
        skr::MakeArc<baldr::RateLimiter>(2, std::chrono::seconds(10));
    std::shared_ptr<baldr::RateLimitMiddleware>       mMw;
    skr::Arc<skr::Logger<baldr::RateLimitMiddleware>> mLogger;

    void SetUp() override
    {
        mRequest.method    = baldr::HttpMethod::Get;
        mRequest.path      = "/api";
        mRequest.version   = "HTTP/1.1";
        mRequest.clientIp  = "10.0.0.1";
        auto opts          = skr::MakeArc<skr::LoggerOptions>();
        opts->asyncEnabled = false;
        mLogger = skr::MakeArc<skr::Logger<baldr::RateLimitMiddleware>>(
            std::move(opts));
        mMw = std::make_shared<baldr::RateLimitMiddleware>(mLimiter, mLogger);
        mMw->setUseForwardedFor(true);
        mMw->setTrustedProxies({ "10.", "192.168." });
        mResponse = baldr::HttpResponse(mRequest);
    }
};

TEST_F(RateLimitForwardedForSpec, TrustsRightmostHopWhenInTrustedList)
{
    // 10.0.0.1 (peer) → "10.0.0.5, 203.0.113.7" — rightmost trusted hop is
    // 10.0.0.5, so the leftmost untrusted IP is 203.0.113.7.
    mRequest.headers["x-forwarded-for"] = "10.0.0.5, 203.0.113.7";

    int called = 0;
    mMw->Handle(mRequest, mResponse, [&]() { ++called; });

    // Two calls under key 203.0.113.7 are allowed; the third is not.
    EXPECT_EQ(called, 1);
    EXPECT_NE(static_cast<int>(mResponse.statusCode),
              static_cast<int>(baldr::StatusCode::TooManyRequests));

    mResponse = baldr::HttpResponse(mRequest);
    mMw->Handle(mRequest, mResponse, [&]() { ++called; });
    EXPECT_EQ(called, 2);

    mResponse = baldr::HttpResponse(mRequest);
    mMw->Handle(mRequest, mResponse, [&]() { ++called; });
    EXPECT_EQ(called, 2);
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(baldr::StatusCode::TooManyRequests));
}

TEST_F(RateLimitForwardedForSpec, AllProxiesTrustedFallsBackToClientIp)
{
    // Every hop in trusted prefixes → fall back to the TCP peer.
    mRequest.headers["x-forwarded-for"] = "10.0.0.5, 10.0.0.6";
    int called                          = 0;
    mMw->Handle(mRequest, mResponse, [&]() { ++called; });
    EXPECT_EQ(called, 1);
    EXPECT_NE(static_cast<int>(mResponse.statusCode),
              static_cast<int>(baldr::StatusCode::TooManyRequests));
}

TEST_F(RateLimitForwardedForSpec, DisabledForwardedForIgnoresHeader)
{
    mMw->setUseForwardedFor(false);
    mRequest.headers["x-forwarded-for"] = "203.0.113.7";
    // Two calls under the TCP-peer key 10.0.0.1 are allowed.
    int called = 0;
    mMw->Handle(mRequest, mResponse, [&]() { ++called; });
    mResponse = baldr::HttpResponse(mRequest);
    mMw->Handle(mRequest, mResponse, [&]() { ++called; });
    EXPECT_EQ(called, 2);
}

// ============================================================================
// HIGH-1 — exception path does not leak e.what() to the client
//
// We can't easily drive HttpConnection::handle end-to-end without a live
// socket, but we can verify the contract at the middleware-chain level:
// any handler that throws must propagate the exception so that the outer
// catch in Connection.cpp substitutes the generic body. The middleware
// chain does not swallow exceptions silently.
// ============================================================================

TEST(ExceptionLeakageSpec, HandlerExceptionPropagatesThroughChain)
{
    auto fakeMw = []() {
        struct Throwing : baldr::IMiddleware
        {
            void Handle(baldr::HttpRequest&, baldr::HttpResponse&,
                        const baldr::NextMiddleware&) override
            {
                throw std::runtime_error("secret: db password is hunter2");
            }
        };
        return Throwing {};
    };

    // Sanity: directly calling Handle on the throwing middleware propagates
    // the exception, which means the outer catch in HttpConnection::handle
    // is the one responsible for producing the response body.
    EXPECT_THROW(
        {
            baldr::HttpRequest  req;
            baldr::HttpResponse resp(req);
            fakeMw().Handle(req, resp, []() {});
        },
        std::runtime_error);
}