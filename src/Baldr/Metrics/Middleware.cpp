#include <Baldr/Detail/Namespace.hpp>
#include <Baldr/Metrics/Middleware.hpp>

#include <chrono>
#include <string>
#include <utility>

#include <Baldr/Application/WebApplication.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Metrics/Registry.hpp>

namespace BALDR_NAMESPACE {

void MetricsMiddleware::Handle(HttpRequest&          request,
                               HttpResponse&         response,
                               const NextMiddleware& next)
{
    auto& reg = BALDR_NAMESPACE::MetricsRegistry::instance();
    reg.incInFlight(+1);

    using namespace std::chrono;
    auto begin = steady_clock::now();

    next();

    auto end = steady_clock::now();
    auto dur = duration<double>(end - begin).count();

    std::string methodLabel = std::string(refl::enum_to_string(request.method));
    std::string pathLabel   = mOptions.usePathLabel ? request.path : "/";

    reg.incRequest(methodLabel, static_cast<int>(response.statusCode));
    reg.observeLatencySeconds(methodLabel, pathLabel, dur);

    reg.incInFlight(-1);
}

void MapMetrics(WebApplication& app, MetricsOptions options)
{
    if (options.endpointPath.empty())
        return;

    const std::string path = options.endpointPath;

    app.MapGet(path, [path](HttpResponse& response) -> ContentResult {
        (void) path;
        auto body = BALDR_NAMESPACE::MetricsRegistry::instance().renderPrometheus();
        ContentResult r(body, "text/plain; version=0.0.4", StatusCode::OK);
        return r;
    });
}

} // namespace BALDR_NAMESPACE
