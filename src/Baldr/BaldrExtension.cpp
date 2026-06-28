#include "BaldrExtension.hpp"

#include "HttpRequestParser.hpp"
#include "HttpServer.hpp"
#include "LoggingMiddleware.hpp"
#include "RateLimitMiddleware.hpp"
#include "Skirnir/Common.hpp"
#include "WebApplication.hpp"

void BaldrExtension::ConfigureServices(skr::ServiceCollection& services)
{
    services.AddSingleton<MiddlewareProvider>();
    services.AddSingleton<Router>();
    services.AddSingleton<HttpServerOptions>();
    services.AddSingleton<HttpServer>();

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
