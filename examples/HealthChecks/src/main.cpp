#include <Baldr/Baldr.hpp>

class DatabaseHealthCheck : public baldr::IHealthCheck
{
  public:
    std::string_view CheckName() const noexcept override { return "db"; }
    bool             Check(const baldr::HttpRequest&) override { return true; }
};

class CacheHealthCheck : public baldr::IHealthCheck
{
  public:
    std::string_view CheckName() const noexcept override { return "cache"; }
    bool             Check(const baldr::HttpRequest&) override { return true; }
};

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