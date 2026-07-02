#include "BaldrExtension.hpp"

#include "HttpRequestParser.hpp"
#include "HttpServer.hpp"
#include "InFlightTracker.hpp"
#include "LoggingMiddleware.hpp"
#include "RateLimitMiddleware.hpp"
#include "Skirnir/Common.hpp"
#include "WebApplication.hpp"
#include "WorkerPool.hpp"

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
