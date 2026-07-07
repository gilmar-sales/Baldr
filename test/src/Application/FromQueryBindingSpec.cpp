#include <Baldr/Http/Connection.hpp>
#include <Baldr/Http/FromQuery.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Router.hpp>

#include <atomic>
#include <string>

namespace
{
    struct SearchFilters
    {
        std::string name;
        int         age = 0;
    };
} // namespace

class FromQueryBindingSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method   = baldr::HttpMethod::Get;
        mRequest.path     = "/search";
        mRequest.version  = "HTTP/1.1";
        mRequest.clientIp = "127.0.0.1";
        mResponse         = baldr::HttpResponse(mRequest);
    }

    baldr::HttpRequest             mRequest;
    baldr::HttpResponse            mResponse;
    skr::Arc<skr::ServiceProvider> mEmptyProvider =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
};

TEST_F(FromQueryBindingSpec, RouterBindsValidQueryToFromQueryParameter)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> wasOk { false };
    std::string       capturedName;
    int               capturedAge = -1;

    router->MapRoute(
        baldr::HttpMethod::Get, "/search", std::string {},
        baldr::RouteOptions {}, [&](baldr::FromQuery<SearchFilters> q) {
            wasOk        = q.isOk();
            capturedName = q.value.name;
            capturedAge  = q.value.age;
        });

    auto match = router->match(baldr::HttpMethod::Get, "/search");
    ASSERT_TRUE(match.has_value());

    baldr::HttpRequest request = mRequest;
    request.query["name"]      = std::string("Alice");
    request.query["age"]       = std::string("30");

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_TRUE(wasOk.load());
    EXPECT_EQ(capturedName, "Alice");
    EXPECT_EQ(capturedAge, 30);
}

TEST_F(FromQueryBindingSpec, RouterReportsMissingQueryField)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> handlerRan { false };

    router->MapRoute(
        baldr::HttpMethod::Get, "/search", std::string {},
        baldr::RouteOptions {},
        [&](baldr::FromQuery<SearchFilters> /*q*/) { handlerRan = true; });

    auto match = router->match(baldr::HttpMethod::Get, "/search");
    ASSERT_TRUE(match.has_value());

    baldr::HttpRequest request = mRequest;
    request.query["name"]      = std::string("Bob");
    // "age" missing

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_FALSE(handlerRan.load());
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_EQ(mResponse.headers.at("Content-Type"), "application/json");
    EXPECT_NE(mResponse.body.find("\"field\":\"age\""), std::string::npos);
    EXPECT_NE(mResponse.body.find("query"), std::string::npos);
}

TEST_F(FromQueryBindingSpec, RouterBindsFromQueryAlongsideHttpRequest)
{
    auto router = skr::MakeArc<baldr::Router>();

    std::atomic<bool> wasOk { false };
    std::string       capturedPath;
    std::string       capturedName;

    router->MapRoute(
        baldr::HttpMethod::Get, "/search", std::string {},
        baldr::RouteOptions {},
        [&](baldr::HttpRequest& request, baldr::FromQuery<SearchFilters> q) {
            capturedPath = request.path;
            wasOk        = q.isOk();
            capturedName = q.value.name;
        });

    auto match = router->match(baldr::HttpMethod::Get, "/search");
    ASSERT_TRUE(match.has_value());

    baldr::HttpRequest request = mRequest;
    request.query["name"]      = std::string("Carol");
    request.query["age"]       = std::string("42");

    baldr::MiddlewareFactoryList factories;
    baldr::HttpConnection::runMiddlewareChain(
        factories, mEmptyProvider, request, mResponse, match->handler);

    EXPECT_EQ(capturedPath, "/search");
    EXPECT_TRUE(wasOk.load());
    EXPECT_EQ(capturedName, "Carol");
}
