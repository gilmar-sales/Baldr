#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/StatusCode.hpp>
#include <Baldr/Middleware/Cors.hpp>
#include <Baldr/Middleware/ExceptionHandler.hpp>
#include <Baldr/Middleware/RequestId.hpp>

#include <stdexcept>

class MiddlewareSpec : public ::testing::Test
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

TEST_F(MiddlewareSpec, CorsMiddlewareSetsAccessControlHeaders)
{
    baldr::CorsMiddleware cors;
    bool                  nextCalled = false;
    cors.Handle(mRequest, mResponse, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(mResponse.headers.at("Access-Control-Allow-Origin"), "*");
    EXPECT_NE(mResponse.headers.find("Access-Control-Allow-Methods"),
              mResponse.headers.end());
}

TEST_F(MiddlewareSpec,
       CorsMiddlewareWithCredentialsDoesNotEmitWildcardOrigin)
{
    baldr::CorsOptions opts;
    opts.allowCredentials = true;
    baldr::CorsMiddleware cors(std::move(opts));
    mRequest.headers["origin"] = "https://app.example.com";
    cors.Handle(mRequest, mResponse, [&]() {});

    EXPECT_EQ(mResponse.headers.at("Access-Control-Allow-Origin"),
              "https://app.example.com");
    EXPECT_EQ(mResponse.headers.at("Access-Control-Allow-Credentials"),
              "true");
    EXPECT_EQ(mResponse.headers.at("Vary"), "Origin");
}

TEST_F(MiddlewareSpec, CorsMiddlewareDefaultNoOriginHeaderOmitsOriginHeader)
{
    baldr::CorsOptions opts;
    opts.allowCredentials = true;
    baldr::CorsMiddleware cors(std::move(opts));
    cors.Handle(mRequest, mResponse, [&]() {});

    EXPECT_FALSE(mResponse.headers.contains("Access-Control-Allow-Origin"));
    EXPECT_FALSE(mResponse.headers.contains("Access-Control-Allow-Credentials"));
}

TEST_F(MiddlewareSpec, CorsMiddlewareShortCircuitsOnOptions)
{
    baldr::CorsMiddleware cors;
    bool                  nextCalled = false;
    mRequest.method                  = baldr::HttpMethod::Options;
    cors.Handle(mRequest, mResponse, [&]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(baldr::StatusCode::NoContent));
}

TEST_F(MiddlewareSpec, RequestIdMiddlewareEchoesClientHeader)
{
    baldr::RequestIdMiddleware mw;
    mRequest.headers["x-request-id"] = "abc-123";
    mw.Handle(mRequest, mResponse, [&]() {});

    EXPECT_EQ(mResponse.headers.at("X-Request-ID"), "abc-123");
    EXPECT_EQ(mRequest.headers.at("X-Request-ID"), "abc-123");
}

TEST_F(MiddlewareSpec, RequestIdMiddlewareGeneratesWhenAbsent)
{
    baldr::RequestIdMiddleware mw;
    mw.Handle(mRequest, mResponse, [&]() {});

    EXPECT_FALSE(mResponse.headers.at("X-Request-ID").empty());
}

TEST_F(MiddlewareSpec, ExceptionHandlerCatchesStdException)
{
    baldr::ExceptionHandlerMiddleware mw;
    mw.Handle(mRequest, mResponse, []() { throw std::runtime_error("boom"); });

    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(baldr::StatusCode::InternalServerError));
    EXPECT_EQ(mResponse.body, "Internal Server Error");
}

TEST_F(MiddlewareSpec, ExceptionHandlerIncludesDetailsWhenEnabled)
{
    baldr::ExceptionHandlerOptions opts;
    opts.includeDetailsInDev = true;
    baldr::ExceptionHandlerMiddleware mw(opts);
    mw.Handle(mRequest, mResponse, []() { throw std::runtime_error("boom"); });

    EXPECT_EQ(mResponse.body, "boom");
}

TEST_F(MiddlewareSpec, ExceptionHandlerUsesCustomMapper)
{
    baldr::ExceptionHandlerOptions opts;
    opts.mapper = [](const std::exception&) {
        return std::string("custom-mapped");
    };
    baldr::ExceptionHandlerMiddleware mw(opts);
    mw.Handle(mRequest, mResponse, []() { throw std::runtime_error("boom"); });

    EXPECT_EQ(mResponse.body, "custom-mapped");
    EXPECT_EQ(mResponse.headers.at("Content-Type"), "text/plain");
}

TEST_F(MiddlewareSpec, ExceptionHandlerCatchesUnknownException)
{
    baldr::ExceptionHandlerMiddleware mw;
    mw.Handle(mRequest, mResponse, []() { throw 42; });

    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(baldr::StatusCode::InternalServerError));
    EXPECT_EQ(mResponse.body, "Internal Server Error");
}