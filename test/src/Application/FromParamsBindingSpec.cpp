#include <Baldr/Http/Connection.hpp>
#include <Baldr/Http/FromParams.hpp>
#include <Baldr/Http/FromQuery.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Router.hpp>

#include <atomic>
#include <string>

namespace
{
    struct UserPathArgs
    {
        std::string id;
    };

    struct PostFilters
    {
        int limit = 0;
    };
} // namespace

class FromParamsBindingSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method   = baldr::HttpMethod::Get;
        mRequest.path     = "/users/u-1";
        mRequest.version  = "HTTP/1.1";
        mRequest.clientIp = "127.0.0.1";
        mResponse         = baldr::HttpResponse(mRequest);
    }

    baldr::HttpRequest             mRequest;
    baldr::HttpResponse            mResponse;
    skr::Arc<skr::ServiceProvider> mEmptyProvider =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
};

TEST_F(FromParamsBindingSpec, RouterBindsValidPathParamToFromParamsParameter)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> wasOk { false };
    std::string       capturedId;

    router->MapRoute(
        baldr::HttpMethod::Get, "/users/:id", std::string {},
        baldr::RouteOptions {}, [&](baldr::FromParams<UserPathArgs> p) {
            wasOk      = p.isOk();
            capturedId = p.value.id;
        });

    auto match = router->match(baldr::HttpMethod::Get, "/users/u-42");
    ASSERT_TRUE(match.has_value());

    baldr::HttpRequest request = mRequest;
    request.path               = "/users/u-42";
    request.params             = match->extractRouteParams("/users/u-42");

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_TRUE(wasOk.load());
    EXPECT_EQ(capturedId, "u-42");
}

TEST_F(FromParamsBindingSpec, RouterReportsMissingPathParam)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> wasOk { true };
    std::string       errorMessage;

    router->MapRoute(
        baldr::HttpMethod::Get, "/users/:id", std::string {},
        baldr::RouteOptions {}, [&](baldr::FromParams<UserPathArgs> p) {
            wasOk = p.isOk();
            if (p.error)
                errorMessage = p.error->message;
        });

    auto match = router->match(baldr::HttpMethod::Get, "/users/u-42");
    ASSERT_TRUE(match.has_value());

    baldr::HttpRequest request = mRequest;
    request.path               = "/users/u-42";
    // intentionally do not populate request.params

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_FALSE(wasOk.load());
    EXPECT_FALSE(errorMessage.empty());
    EXPECT_NE(errorMessage.find("path template"), std::string::npos);
}

TEST_F(FromParamsBindingSpec,
       RouterBindsFromParamsAndFromQueryAlongsideHttpRequest)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> wasOk { false };
    std::atomic<bool> queryOk { false };
    std::string       capturedId;
    int               capturedLimit = -1;

    router->MapRoute(
        baldr::HttpMethod::Get, "/users/:id/posts", std::string {},
        baldr::RouteOptions {},
        [&](baldr::HttpRequest& request, baldr::FromParams<UserPathArgs> p,
            baldr::FromQuery<PostFilters> q) {
            (void) request;
            wasOk         = p.isOk();
            queryOk       = q.isOk();
            capturedId    = p.value.id;
            capturedLimit = q.value.limit;
        });

    auto match = router->match(baldr::HttpMethod::Get, "/users/u-7/posts");
    ASSERT_TRUE(match.has_value());

    baldr::HttpRequest request = mRequest;
    request.path               = "/users/u-7/posts";
    request.params             = match->extractRouteParams("/users/u-7/posts");
    request.query["limit"]     = std::string("5");

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_TRUE(wasOk.load());
    EXPECT_TRUE(queryOk.load());
    EXPECT_EQ(capturedId, "u-7");
    EXPECT_EQ(capturedLimit, 5);
}
