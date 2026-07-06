#include <Baldr/Application/HealthCheckResult.hpp>
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

    class DbHealthyCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "db"; }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            return baldr::HealthCheckResult::Healthy();
        }
    };

    class CacheHealthyCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "cache"; }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            return baldr::HealthCheckResult::Healthy();
        }
    };

    class CacheUnhealthyCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "cache"; }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            return baldr::HealthCheckResult::Unhealthy({}, "connection lost");
        }
    };

    class ThrowingCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override
        {
            return "explode";
        }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            throw std::runtime_error("boom");
        }
    };

    class DegradedCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "queue"; }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            return baldr::HealthCheckResult::Degraded("slow consumer",
                                                      std::nullopt,
                                                      R"({"depth":1024})");
        }
    };

    class DataCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override
        {
            return "metrics";
        }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            return baldr::HealthCheckResult::Healthy("telemetry");
        }
    };

    class DbDownCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "db"; }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            return baldr::HealthCheckResult::Unhealthy("primary db",
                                                       "connection refused");
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
        ->AddTransient<baldr::IHealthCheck, DbHealthyCheck>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, CacheHealthyCheck>();
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
    EXPECT_NE(res.body.find("\"db\":{\"status\":\"healthy\""),
              std::string::npos);
    EXPECT_NE(res.body.find("\"cache\":{\"status\":\"healthy\""),
              std::string::npos);
}

TEST(HealthChecksSpec, AnyFailedCheckReturns503)
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, DbHealthyCheck>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, CacheUnhealthyCheck>();
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
    EXPECT_NE(res.body.find("\"cache\":{\"status\":\"unhealthy\""),
              std::string::npos);
    EXPECT_NE(res.body.find("\"error\":\"connection lost\""),
              std::string::npos);
}

TEST(HealthChecksSpec, LivenessPathIsAlwaysHealthy)
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, CacheUnhealthyCheck>();
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
    EXPECT_NE(res.body.find("\"explode\":{\"status\":\"unhealthy\""),
              std::string::npos);
    EXPECT_NE(res.body.find("\"error\":\"boom\""), std::string::npos);
}

TEST(HealthChecksSpec, DegradedCheckStillReturns200)
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, DbHealthyCheck>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, DegradedCheck>();
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
    EXPECT_NE(res.body.find("\"queue\":{\"status\":\"degraded\""),
              std::string::npos);
    EXPECT_NE(res.body.find("\"description\":\"slow consumer\""),
              std::string::npos);
    EXPECT_NE(res.body.find("\"data\":{\"depth\":1024}"), std::string::npos);
}

TEST(HealthChecksSpec, UnhealthyReturns503AndIncludesError)
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, DbDownCheck>();
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
    EXPECT_NE(res.body.find("\"description\":\"primary db\""),
              std::string::npos);
    EXPECT_NE(res.body.find("\"error\":\"connection refused\""),
              std::string::npos);
}

TEST(HealthChecksSpec, CheckWithStructuredData)
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, DataCheck>();
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
    EXPECT_NE(res.body.find("\"metrics\":{\"status\":\"healthy\""),
              std::string::npos);
    EXPECT_NE(res.body.find("\"description\":\"telemetry\""),
              std::string::npos);
}