#include <Baldr/Application/IHealthCheck.hpp>
#include <Baldr/Application/WebApplication.hpp>
#include <Baldr/BaldrExtension.hpp>
#include <Baldr/Http/Router.hpp>
#include <Baldr/Http/ServerOptions.hpp>

#include <Skirnir/DependencyInjection/ApplicationBuilder.hpp>
#include <Skirnir/DependencyInjection/ServiceCollection.hpp>
#include <Skirnir/Logging/Logger.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

namespace
{
    skr::Arc<baldr::WebApplication> buildApp()
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

    void invokeRoute(const baldr::RouteEntry&              entry,
                     const baldr::HttpRequest&             request,
                     const skr::Arc<skr::ServiceProvider>& sp,
                     baldr::HttpResponse&                  out)
    {
        baldr::HttpRequest copy = request;
        (void) entry.extractRouteParams(request.path);
        entry.handler(copy, out, sp);
    }

    class DbOkCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "db"; }
        bool Check(const baldr::HttpRequest&) override { return true; }
    };

    class CacheOkCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "cache"; }
        bool Check(const baldr::HttpRequest&) override { return true; }
    };

    class CacheFailCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "cache"; }
        bool Check(const baldr::HttpRequest&) override { return false; }
    };

    class ThrowingCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override
        {
            return "explode";
        }
        bool Check(const baldr::HttpRequest&) override
        {
            throw std::runtime_error("boom");
        }
    };
} // namespace

TEST(HealthChecksSpec, RegistersAllPathsAndReturns200WithEmptyChecks)
{
    auto app = buildApp();
    app->MapHealthChecks({ "/healthz", "/readyz" });

    auto router = app->GetRouter();
    EXPECT_TRUE(router->match(baldr::HttpMethod::Get, "/healthz").has_value());
    EXPECT_TRUE(router->match(baldr::HttpMethod::Get, "/readyz").has_value());
}

TEST(HealthChecksSpec, EmptyChecksReturnHealthy200)
{
    auto app = buildApp();
    app->MapHealthChecks({ "/healthz" });

    auto router = app->GetRouter();
    auto entry  = router->match(baldr::HttpMethod::Get, "/healthz");
    ASSERT_TRUE(entry.has_value());

    baldr::HttpRequest req;
    req.method = baldr::HttpMethod::Get;
    req.path   = "/healthz";

    baldr::HttpResponse            res(req);
    skr::Arc<skr::ServiceProvider> sp =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
    invokeRoute(*entry, req, sp, res);

    EXPECT_EQ(static_cast<int>(res.statusCode),
              static_cast<int>(baldr::StatusCode::OK));
    EXPECT_NE(res.body.find("\"status\":\"healthy\""), std::string::npos);
}

TEST(HealthChecksSpec, AllChecksPassReturn200)
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, DbOkCheck>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, CacheOkCheck>();
    auto app = builder.Build<baldr::WebApplication>();

    app->MapHealthChecks({ "/healthz" });

    auto entry = app->GetRouter()->match(baldr::HttpMethod::Get, "/healthz");
    ASSERT_TRUE(entry.has_value());

    baldr::HttpRequest req;
    req.method = baldr::HttpMethod::Get;
    req.path   = "/healthz";
    baldr::HttpResponse            res(req);
    skr::Arc<skr::ServiceProvider> sp =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
    invokeRoute(*entry, req, sp, res);

    EXPECT_EQ(static_cast<int>(res.statusCode),
              static_cast<int>(baldr::StatusCode::OK));
    EXPECT_NE(res.body.find("\"status\":\"healthy\""), std::string::npos);
    EXPECT_NE(res.body.find("\"db\":true"), std::string::npos);
    EXPECT_NE(res.body.find("\"cache\":true"), std::string::npos);
}

TEST(HealthChecksSpec, AnyFailedCheckReturns503)
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, DbOkCheck>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, CacheFailCheck>();
    auto app = builder.Build<baldr::WebApplication>();

    app->MapHealthChecks({ "/healthz" });

    auto entry = app->GetRouter()->match(baldr::HttpMethod::Get, "/healthz");
    ASSERT_TRUE(entry.has_value());

    baldr::HttpRequest req;
    req.method = baldr::HttpMethod::Get;
    req.path   = "/healthz";
    baldr::HttpResponse            res(req);
    skr::Arc<skr::ServiceProvider> sp =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
    invokeRoute(*entry, req, sp, res);

    EXPECT_EQ(static_cast<int>(res.statusCode),
              static_cast<int>(baldr::StatusCode::ServiceUnavailable));
    EXPECT_NE(res.body.find("\"status\":\"unhealthy\""), std::string::npos);
    EXPECT_NE(res.body.find("\"cache\":false"), std::string::npos);
}

TEST(HealthChecksSpec, LivenessPathIsAlwaysHealthy)
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, CacheFailCheck>();
    auto app = builder.Build<baldr::WebApplication>();

    app->MapHealthChecks({ "/readyz" }, "/livez");

    auto router = app->GetRouter();
    auto entry  = router->match(baldr::HttpMethod::Get, "/livez");
    ASSERT_TRUE(entry.has_value());

    baldr::HttpRequest req;
    req.method = baldr::HttpMethod::Get;
    req.path   = "/livez";
    baldr::HttpResponse            res(req);
    skr::Arc<skr::ServiceProvider> sp =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
    invokeRoute(*entry, req, sp, res);

    EXPECT_EQ(static_cast<int>(res.statusCode),
              static_cast<int>(baldr::StatusCode::OK));
    EXPECT_NE(res.body.find("\"status\":\"healthy\""), std::string::npos);
}

TEST(HealthChecksSpec, PredicateThrowingTreatedAsUnhealthy)
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, ThrowingCheck>();
    auto app = builder.Build<baldr::WebApplication>();

    app->MapHealthChecks({ "/healthz" });

    auto entry = app->GetRouter()->match(baldr::HttpMethod::Get, "/healthz");
    ASSERT_TRUE(entry.has_value());

    baldr::HttpRequest req;
    req.method = baldr::HttpMethod::Get;
    req.path   = "/healthz";
    baldr::HttpResponse            res(req);
    skr::Arc<skr::ServiceProvider> sp =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
    invokeRoute(*entry, req, sp, res);

    EXPECT_EQ(static_cast<int>(res.statusCode),
              static_cast<int>(baldr::StatusCode::ServiceUnavailable));
    EXPECT_NE(res.body.find("\"explode\":false"), std::string::npos);
}