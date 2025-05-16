#include "Baldr/BaldrExtension.hpp"

#include "Baldr/HttpServer.hpp"
#include "Baldr/LoggingMiddleware.hpp"
#include "Baldr/WebApplication.hpp"

void BaldrExtension::ConfigureServices(skr::ServiceCollection& services)
{
    services.AddSingleton<MiddlewareProvider>();
    services.AddSingleton<PathMatcher>();
    services.AddSingleton<HttpServerOptions>();
    services.AddSingleton<HttpServer>();

    services.AddTransient<skr::Logger<HttpSession>>();
    services.AddScoped<LoggingMiddleware>();
}

void BaldrExtension::UseServices(skr::ServiceProvider& serviceProvider)
{
    auto webApp = serviceProvider.GetService<WebApplication>();

    webApp->Use<LoggingMiddleware>();
}