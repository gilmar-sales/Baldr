#include <Baldr/Middleware/Cors.hpp>
#include <Baldr/Middleware/ExceptionHandler.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Middleware/RequestId.hpp>
#include <Baldr/Http/StatusCode.hpp>

#include <stdexcept>

class MiddlewareSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method  = HttpMethod::Get;
        mRequest.path    = "/";
        mRequest.version = "HTTP/1.1";
        mResponse        = HttpResponse(mRequest);
    }

    HttpRequest  mRequest;
    HttpResponse mResponse;
};

TEST_F(MiddlewareSpec, CorsMiddlewareSetsAccessControlHeaders)
{
    CorsMiddleware cors;
    bool           nextCalled = false;
    cors.Handle(
        mRequest, mResponse,
        [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(mResponse.headers.at("Access-Control-Allow-Origin"), "*");
    EXPECT_NE(mResponse.headers.find("Access-Control-Allow-Methods"),
              mResponse.headers.end());
}

TEST_F(MiddlewareSpec, CorsMiddlewareShortCircuitsOnOptions)
{
    CorsMiddleware cors;
    bool           nextCalled = false;
    mRequest.method           = HttpMethod::Options;
    cors.Handle(
        mRequest, mResponse,
        [&]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::NoContent));
}

TEST_F(MiddlewareSpec, RequestIdMiddlewareEchoesClientHeader)
{
    RequestIdMiddleware mw;
    mRequest.headers["x-request-id"] = "abc-123";
    mw.Handle(
        mRequest, mResponse, [&]() {});

    EXPECT_EQ(mResponse.headers.at("X-Request-ID"), "abc-123");
    EXPECT_EQ(mRequest.headers.at("X-Request-ID"), "abc-123");
}

TEST_F(MiddlewareSpec, RequestIdMiddlewareGeneratesWhenAbsent)
{
    RequestIdMiddleware mw;
    mw.Handle(
        mRequest, mResponse, [&]() {});

    EXPECT_FALSE(mResponse.headers.at("X-Request-ID").empty());
}

TEST_F(MiddlewareSpec, ExceptionHandlerCatchesStdException)
{
    ExceptionHandlerMiddleware mw;
    mw.Handle(
        mRequest, mResponse, []() {
            throw std::runtime_error("boom");
        });

    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::InternalServerError));
    EXPECT_EQ(mResponse.body, "Internal Server Error");
}

TEST_F(MiddlewareSpec, ExceptionHandlerIncludesDetailsWhenEnabled)
{
    ExceptionHandlerOptions opts;
    opts.includeDetailsInDev = true;
    ExceptionHandlerMiddleware mw(opts);
    mw.Handle(
        mRequest, mResponse, []() {
            throw std::runtime_error("boom");
        });

    EXPECT_EQ(mResponse.body, "boom");
}

TEST_F(MiddlewareSpec, ExceptionHandlerUsesCustomMapper)
{
    ExceptionHandlerOptions opts;
    opts.mapper = [](const std::exception&) {
        return std::string("custom-mapped");
    };
    ExceptionHandlerMiddleware mw(opts);
    mw.Handle(
        mRequest, mResponse, []() {
            throw std::runtime_error("boom");
        });

    EXPECT_EQ(mResponse.body, "custom-mapped");
    EXPECT_EQ(mResponse.headers.at("Content-Type"), "text/plain");
}

TEST_F(MiddlewareSpec, ExceptionHandlerCatchesUnknownException)
{
    ExceptionHandlerMiddleware mw;
    mw.Handle(
        mRequest, mResponse, []() {
            throw 42;
        });

    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::InternalServerError));
    EXPECT_EQ(mResponse.body, "Internal Server Error");
}