#include "BaldrExtension.hpp"

#include "BufferPool.hpp"
#include "HttpRequestParser.hpp"
#include "HttpServer.hpp"
#include "LoggingMiddleware.hpp"
#include "MpMcPool.hpp"
#include "RateLimitMiddleware.hpp"
#include "Skirnir/Common.hpp"
#include "WebApplication.hpp"

void BaldrExtension::ConfigureServices(skr::ServiceCollection& services)
{
    services.AddSingleton<MiddlewareProvider>();
    services.AddSingleton<Router>();
    services.AddSingleton<HttpServerOptions>();
    services.AddSingleton<HttpServer>();
    services.AddSingleton<ReadBufferPool>();
    services.AddSingleton(skr::MakeRef<MpMcBufferPool>(size_t(16 * 1024)));

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
