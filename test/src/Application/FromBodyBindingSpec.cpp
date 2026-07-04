#include <Baldr/Http/Connection.hpp>
#include <Baldr/Http/FromBody.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Router.hpp>

#include <atomic>
#include <string>

#include "../Http/UserDto.hpp"

namespace
{
    struct LoginDto
    {
        std::string username;
        std::string password;
    };
} // namespace

class FromBodyBindingSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method   = baldr::HttpMethod::Post;
        mRequest.path     = "/login";
        mRequest.version  = "HTTP/1.1";
        mRequest.clientIp = "127.0.0.1";
        mResponse         = baldr::HttpResponse(mRequest);
    }

    baldr::HttpRequest             mRequest;
    baldr::HttpResponse            mResponse;
    skr::Arc<skr::ServiceProvider> mEmptyProvider =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
};

TEST_F(FromBodyBindingSpec, RouterBindsValidJsonBodyToFromBodyParameter)
{
    auto router = skr::MakeArc<baldr::Router>();
    router->MapRoute(baldr::HttpMethod::Post, "/login", std::string {},
                     baldr::RouteOptions {}, [](baldr::FromBody<UserDto> body) {
                         // Captured via the response inside the test scope
                         // rather than a free variable to avoid static state.
                     });

    auto match = router->match(baldr::HttpMethod::Post, "/login");
    ASSERT_TRUE(match.has_value());

    std::atomic<bool> handlerRan { false };
    std::atomic<bool> wasOk { false };
    std::string       capturedName;
    int               capturedAge = -1;

    baldr::RouteHandler finalHandler =
        [&, handler = std::move(match->handler)](
            baldr::HttpRequest& request, baldr::HttpResponse& response,
            const skr::Arc<skr::ServiceProvider>& sp) {
            handler(request, response, sp);
            handlerRan = true;
        };

    baldr::HttpRequest request      = mRequest;
    request.body                    = R"({"name":"Alice","age":30})";
    request.headers["content-type"] = "application/json";

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, finalHandler);

    ASSERT_TRUE(handlerRan.load());
}

TEST_F(FromBodyBindingSpec, RouterBindsInvalidJsonBodyAndStillRunsHandler)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> wasOk { false };
    std::atomic<bool> hasError { false };
    std::atomic<int>  errorStatus { 0 };
    std::string       errorMessage;

    router->MapRoute(
        baldr::HttpMethod::Post, "/login", std::string {},
        baldr::RouteOptions {}, [&](baldr::FromBody<UserDto> body) {
            wasOk    = body.isOk();
            hasError = body.error.has_value();
            if (body.error)
            {
                errorStatus  = static_cast<int>(body.error->statusCode);
                errorMessage = body.error->message;
            }
        });

    auto match = router->match(baldr::HttpMethod::Post, "/login");
    ASSERT_TRUE(match.has_value());

    baldr::HttpRequest request      = mRequest;
    request.body                    = "not-json-at-all";
    request.headers["content-type"] = "application/json";

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_FALSE(wasOk.load());
    EXPECT_TRUE(hasError.load());
    EXPECT_EQ(errorStatus.load(),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_FALSE(errorMessage.empty());
}

TEST_F(FromBodyBindingSpec, RouterBindsBodyWhenContentTypeHeaderIsMissing)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> wasOk { false };
    std::string       capturedName;
    int               capturedAge = -1;

    router->MapRoute(
        baldr::HttpMethod::Post, "/login", std::string {},
        baldr::RouteOptions {}, [&](baldr::FromBody<UserDto> body) {
            wasOk        = body.isOk();
            capturedName = body.value.name;
            capturedAge  = body.value.age;
        });

    auto match = router->match(baldr::HttpMethod::Post, "/login");
    ASSERT_TRUE(match.has_value());

    baldr::HttpRequest request = mRequest;
    request.body               = R"({"name":"Bob","age":42})";

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_TRUE(wasOk.load());
    EXPECT_EQ(capturedName, "Bob");
    EXPECT_EQ(capturedAge, 42);
}

TEST_F(FromBodyBindingSpec, RouterBindsBodyAlongsideOtherParameters)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> wasOk { false };
    std::string       capturedPath;
    std::string       capturedName;
    int               capturedAge = -1;

    router->MapRoute(
        baldr::HttpMethod::Post, "/echo/:slug", std::string {},
        baldr::RouteOptions {},
        [&](baldr::HttpRequest& request, baldr::FromBody<UserDto> body) {
            capturedPath = request.params["slug"];
            wasOk        = body.isOk();
            capturedName = body.value.name;
            capturedAge  = body.value.age;
        });

    auto match = router->match(baldr::HttpMethod::Post, "/echo/abc");
    ASSERT_TRUE(match.has_value());

    auto               params       = match->extractRouteParams("/echo/abc");
    baldr::HttpRequest request      = mRequest;
    request.path                    = "/echo/abc";
    request.params                  = std::move(params);
    request.body                    = R"({"name":"Carol","age":21})";
    request.headers["content-type"] = "application/json";

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_EQ(capturedPath, "abc");
    EXPECT_TRUE(wasOk.load());
    EXPECT_EQ(capturedName, "Carol");
    EXPECT_EQ(capturedAge, 21);
}

TEST_F(FromBodyBindingSpec, RouterRejectsUnsupportedContentType)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> wasOk { true };
    std::atomic<int>  errorStatus { 0 };

    router->MapRoute(
        baldr::HttpMethod::Post, "/login", std::string {},
        baldr::RouteOptions {}, [&](baldr::FromBody<LoginDto> body) {
            wasOk = body.isOk();
            if (body.error)
                errorStatus = static_cast<int>(body.error->statusCode);
        });

    auto match = router->match(baldr::HttpMethod::Post, "/login");
    ASSERT_TRUE(match.has_value());

    baldr::HttpRequest request      = mRequest;
    request.headers["content-type"] = "text/plain";
    request.body                    = R"({"username":"a","password":"b"})";

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_FALSE(wasOk.load());
    EXPECT_EQ(errorStatus.load(),
              static_cast<int>(baldr::StatusCode::UnsupportedMediaType));
}