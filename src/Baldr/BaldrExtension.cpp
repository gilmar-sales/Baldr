#include <Baldr/Detail/Namespace.hpp>
#include "BaldrExtension.hpp"

#include <Baldr/Application/InFlightTracker.hpp>
#include <Baldr/Application/WebApplication.hpp>
#include <Baldr/Application/WorkerPool.hpp>
#include <Baldr/Http/Connection.hpp>
#include <Baldr/Http/RequestParser.hpp>
#include <Baldr/Http/Server.hpp>
#include <Baldr/Middleware/Logging.hpp>
#include <Baldr/Middleware/RateLimit/Middleware.hpp>

namespace BALDR_NAMESPACE {

void BaldrExtension::ConfigureServices(skr::ServiceCollection& services)
{
    services.AddSingleton<MiddlewareProvider>();
    services.AddSingleton<Router>();
    services.AddSingleton<HttpServerOptions>();
    services.AddSingleton<InFlightTracker>();
    services.AddSingleton<HttpServer>();
    services.AddSingleton(skr::MakeArc<WorkerPool>(
        std::max<std::size_t>(1, std::thread::hardware_concurrency())));

    services.AddTransient<skr::Logger<HttpConnection>>();
    services.AddTransient<skr::Logger<WebApplication>>();
    services.AddTransient<HttpRequestParser>();
    services.AddSingleton(
        skr::MakeArc<RateLimiter>(10, std::chrono::seconds(10)));
    services.AddScoped<LoggingMiddleware>();
    services.AddScoped<RateLimitMiddleware>();
}

void BaldrExtension::UseServices(skr::ServiceProvider& serviceProvider)
{
    auto webApp = serviceProvider.GetService<WebApplication>();

    webApp->Use<LoggingMiddleware>();
}

} // namespace BALDR_NAMESPACE
