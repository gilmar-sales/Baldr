#include <Baldr/Baldr.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
    std::atomic<int> gRequestCount { 0 };

    bool cacheUp(const baldr::HttpRequest&)
    {
        return true;
    }

    bool dbUp(const baldr::HttpRequest&)
    {
        ++gRequestCount;
        return true;
    }
} // namespace

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet("/", [](baldr::HttpRequest&) { return std::string("ok"); });

    app->MapHealthChecks({ "/healthz", "/readyz" },
                         { { "db", &dbUp }, { "cache", &cacheUp } },
                         "/livez");

    app->Run();

    return 0;
}