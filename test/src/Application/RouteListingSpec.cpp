#include <Baldr/Application/RouteListing.hpp>
#include <Baldr/Application/WebApplication.hpp>
#include <Baldr/BaldrExtension.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Router.hpp>

#include <Skirnir/DependencyInjection/ApplicationBuilder.hpp>
#include <Skirnir/Logging/Logger.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
    skr::Arc<baldr::WebApplication> buildListingApp()
    {
        auto builder = skr::ApplicationBuilder();
        builder.WithExtension<baldr::BaldrExtension>(
            [](baldr::BaldrExtension&) {});
        auto logger          = skr::MakeArc<skr::LoggerOptions>();
        logger->asyncEnabled = false;
        builder.GetServiceCollection()->AddSingleton<skr::LoggerOptions>(
            logger);
        return builder.Build<baldr::WebApplication>();
    }
} // namespace

#ifndef NDEBUG

TEST(RouteListingSpec, EndpointIsRegisteredInDebugBuilds)
{
    auto app = buildListingApp();
    app->EnableRouteListing();

    auto router = app->GetRouter();
    EXPECT_TRUE(router->match(baldr::HttpMethod::Get, "/__routes").has_value());
}

TEST(RouteListingSpec, HonorsCustomPath)
{
    auto app = buildListingApp();
    app->EnableRouteListing("/_debug/routes");

    auto router = app->GetRouter();
    EXPECT_TRUE(
        router->match(baldr::HttpMethod::Get, "/_debug/routes").has_value());
    EXPECT_FALSE(
        router->match(baldr::HttpMethod::Get, "/__routes").has_value());
}

TEST(RouteListingSpec, JsonContainsEveryRegisteredRoute)
{
    auto app = buildListingApp();
    app->MapGet("/users", []() { return std::string("ok"); });
    app->MapPost("/users", []() { return std::string("ok"); });
    app->EnableRouteListing();

    auto router = app->GetRouter();
    auto entry  = router->match(baldr::HttpMethod::Get, "/__routes");
    ASSERT_TRUE(entry.has_value());

    baldr::HttpRequest req;
    req.method = baldr::HttpMethod::Get;
    req.path   = "/__routes";

    baldr::HttpResponse            res(req);
    skr::Arc<skr::ServiceProvider> sp =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
    baldr::HttpRequest copy = req;
    (void) entry->extractRouteParams(req.path);
    entry->handler(copy, res, sp);

    EXPECT_NE(res.body.find("\"Get\""), std::string::npos);
    EXPECT_NE(res.body.find("\"Post\""), std::string::npos);
    EXPECT_NE(res.body.find("\"/users\""), std::string::npos);
}

TEST(RouteListingSpec, JsonExposesGroupPrefix)
{
    auto app = buildListingApp();
    app->MapGroup("/api/v1", [](auto& group) {
        group.MapGet("/health", []() { return std::string("ok"); });
    });
    app->EnableRouteListing();

    auto router = app->GetRouter();
    auto entry  = router->match(baldr::HttpMethod::Get, "/__routes");
    ASSERT_TRUE(entry.has_value());

    baldr::HttpRequest req;
    req.method = baldr::HttpMethod::Get;
    req.path   = "/__routes";
    baldr::HttpResponse            res(req);
    skr::Arc<skr::ServiceProvider> sp =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
    baldr::HttpRequest copy = req;
    (void) entry->extractRouteParams(req.path);
    entry->handler(copy, res, sp);

    EXPECT_NE(res.body.find("\"/api/v1/health\""), std::string::npos);
    EXPECT_NE(res.body.find("\"group\":\"/api/v1\""), std::string::npos);
}

TEST(RouteListingSpec, JsonExposesRouteMetadata)
{
    auto app = buildListingApp();
    app->MapGet("/users").WithMetadata("rate-limit", "100/min").Handle([]() {
        return std::string("ok");
    });
    app->EnableRouteListing();

    auto router = app->GetRouter();
    auto entry  = router->match(baldr::HttpMethod::Get, "/__routes");
    ASSERT_TRUE(entry.has_value());

    baldr::HttpRequest req;
    req.method = baldr::HttpMethod::Get;
    req.path   = "/__routes";
    baldr::HttpResponse            res(req);
    skr::Arc<skr::ServiceProvider> sp =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
    baldr::HttpRequest copy = req;
    (void) entry->extractRouteParams(req.path);
    entry->handler(copy, res, sp);

    EXPECT_NE(res.body.find("\"rate-limit\":\"100/min\""), std::string::npos);
}

#endif // !NDEBUG

TEST(RouteListingJsonHelperSpec, EmptyRouterProducesEmptyArray)
{
#ifndef NDEBUG
    std::vector<baldr::RouteEntry> entries;
    auto                           json = baldr::RouteListingToJson(entries);
    EXPECT_EQ(json, R"({"routes":[]})");
#else
    GTEST_SKIP() << "RouteListingToJson is debug-only";
#endif
}