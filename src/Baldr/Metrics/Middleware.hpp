#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE {

struct MetricsOptions
{
    // Path where this middleware will register a /metrics endpoint via
    // WebApplication::MapGet. Defaults to "/metrics". Set to empty to
    // disable mapping (metrics are still collected internally).
    std::string endpointPath = "/metrics";

    // Path label used in the latency histogram. Defaults to the
    // request's path; you can plug in a Normalizer to bucket URLs.
    bool usePathLabel = true;
};

// Records per-request metrics in the process-wide MetricsRegistry.
// Designed to be registered as a service through DI; the BaldrExtension
// can opt-in via `app.Use<MetricsMiddleware>()` and
// `app.MapMetrics(...)`.
class MetricsMiddleware final : public IMiddleware
{
  public:
    explicit MetricsMiddleware(MetricsOptions options = {}) :
        mOptions(std::move(options))
    {
    }

    ~MetricsMiddleware() override = default;

    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override;

  private:
    MetricsOptions mOptions;
};

// Convenience: registers the /metrics endpoint on the WebApplication.
// The handler runs after the chain completes, so the counters reflect
// the final status.
void MapMetrics(class WebApplication& app, MetricsOptions options = {});

} // namespace BALDR_NAMESPACE
