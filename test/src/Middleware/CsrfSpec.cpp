#include <Baldr/Middleware/Csrf.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/StatusCode.hpp>

class CsrfMiddlewareSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method  = HttpMethod::Get;
        mRequest.path    = "/api/items";
        mRequest.version = "HTTP/1.1";
        mResponse        = HttpResponse(mRequest);
    }

    HttpRequest  mRequest;
    HttpResponse mResponse;
};

TEST_F(CsrfMiddlewareSpec, IssuesCookieOnSafeRequestWhenMissing)
{
    CsrfMiddleware mw;
    bool           nextCalled = false;
    mw.Handle(mRequest, mResponse, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    ASSERT_EQ(mResponse.cookies.count("XSRF-TOKEN"), 1u);
    EXPECT_FALSE(mResponse.cookies.at("XSRF-TOKEN").value.empty());
}

TEST_F(CsrfMiddlewareSpec, DoesNotIssueCookieOnSafeRequestWhenPresent)
{
    CsrfMiddleware mw;
    mRequest.cookies["XSRF-TOKEN"] = "existing-token";
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.cookies.count("XSRF-TOKEN"), 0u);
}

TEST_F(CsrfMiddlewareSpec, AllowsUnsafeWhenHeaderMatchesCookie)
{
    CsrfMiddleware mw;
    mRequest.method               = HttpMethod::Post;
    mRequest.cookies["XSRF-TOKEN"] = "abc";
    mRequest.headers["x-xsrf-token"] = "abc";
    bool nextCalled = false;
    mw.Handle(mRequest, mResponse, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_NE(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::Forbidden));
}

TEST_F(CsrfMiddlewareSpec, RejectsUnsafeWhenHeaderMissing)
{
    CsrfMiddleware mw;
    mRequest.method                = HttpMethod::Post;
    mRequest.cookies["XSRF-TOKEN"] = "abc";
    bool nextCalled = false;
    mw.Handle(mRequest, mResponse, [&]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::Forbidden));
}

TEST_F(CsrfMiddlewareSpec, RejectsUnsafeWhenCookieMissing)
{
    CsrfMiddleware mw;
    mRequest.method                  = HttpMethod::Post;
    mRequest.headers["x-xsrf-token"] = "abc";
    bool nextCalled = false;
    mw.Handle(mRequest, mResponse, [&]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::Forbidden));
}

TEST_F(CsrfMiddlewareSpec, RejectsUnsafeWhenTokensDoNotMatch)
{
    CsrfMiddleware mw;
    mRequest.method                  = HttpMethod::Post;
    mRequest.cookies["XSRF-TOKEN"]   = "abc";
    mRequest.headers["x-xsrf-token"] = "xyz";
    bool nextCalled = false;
    mw.Handle(mRequest, mResponse, [&]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::Forbidden));
}

TEST_F(CsrfMiddlewareSpec, RejectsUnsafeWithDifferentLengthTokens)
{
    CsrfMiddleware mw;
    mRequest.method                  = HttpMethod::Post;
    mRequest.cookies["XSRF-TOKEN"]   = "abcd";
    mRequest.headers["x-xsrf-token"] = "abc";
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::Forbidden));
}

TEST_F(CsrfMiddlewareSpec, ExemptPathPrefixBypassesUnsafeCheck)
{
    CsrfOptions opts;
    opts.exemptPathPrefixes = {"/api/webhooks/"};
    CsrfMiddleware mw(opts);

    mRequest.method = HttpMethod::Post;
    mRequest.path   = "/api/webhooks/stripe";
    bool nextCalled = false;
    mw.Handle(mRequest, mResponse, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_NE(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::Forbidden));
}

TEST_F(CsrfMiddlewareSpec, GetIsSafeAndNotSubjectToCheck)
{
    CsrfMiddleware mw;
    mRequest.cookies["XSRF-TOKEN"]   = "abc";
    mRequest.headers["x-xsrf-token"] = "different";
    bool nextCalled = false;
    mw.Handle(mRequest, mResponse, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_NE(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::Forbidden));
}
