#include "Baldr/BaldrExtension.hpp"

#include "Baldr/HttpServer.hpp"
#include "Baldr/LoggingMiddleware.hpp"
#include "Baldr/WebApplication.hpp"
#include "HttpRequestParser.hpp"
#include "RateLimitMiddleware.hpp"

void BaldrExtension::ConfigureServices(skr::ServiceCollection& services)
{
    services.AddSingleton<MiddlewareProvider>();
    services.AddSingleton<Router>();
    services.AddSingleton<HttpServerOptions>();
    services.AddSingleton<HttpServer>();

    services.AddTransient<skr::Logger<HttpConnection>>();
    services.AddSingleton(
        std::make_shared<RateLimiter>(10, std::chrono::seconds(10)));
    services.AddScoped<LoggingMiddleware>();
    services.AddScoped<RateLimitMiddleware>();
    services.AddScoped<HttpRequestParser>();
}

void BaldrExtension::UseServices(skr::ServiceProvider& serviceProvider)
{
    auto webApp = serviceProvider.GetService<WebApplication>();

    webApp->Use<LoggingMiddleware>();
}
