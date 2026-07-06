#include <Baldr/Baldr.hpp>

namespace
{
    class DatabaseHealthCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "db"; }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            return baldr::HealthCheckResult::Unhealthy(
                "primary database", "connection refused",
                R"({"host":"db.internal","port":5432})");
        }
    };

    class CacheHealthCheck : public baldr::IHealthCheck
    {
      public:
        std::string_view CheckName() const noexcept override { return "cache"; }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            return baldr::HealthCheckResult::Degraded(
                "redis replica", std::nullopt, R"({"hitRatio":0.42})");
        }
    };
} // namespace

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, DatabaseHealthCheck>();
    builder.GetServiceCollection()
        ->AddTransient<baldr::IHealthCheck, CacheHealthCheck>();

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet("/", [](baldr::HttpRequest&) { return std::string("ok"); });

    app->MapHealthChecks({ "/healthz", "/readyz" }, "/livez");

    app->Run();

    return 0;
}