#include <chrono>
#include <iostream>
#include <ranges>

#include "PathMatcher.hpp"
#include "RateLimitMiddleware.h"
#include "WebApplication.hpp"
#include "WebApplicationBuilder.hpp"

class LoggingMiddleware final : public IMiddleware
{
  public:
    void Handle(const HttpRequest& request, HttpResponse& response,
                NextMiddleware& next) override
    {
        std::cout << "Request received: " << request.version << " "
                  << request.method << " " << request.path << std::endl;

        next();
    }

    ~LoggingMiddleware() override
    {
        std::cout << "Logging finished" << std::endl;
    }
};

int main()
{
    auto builder = WebApplication::CreateBuilder();

    builder.GetServiceCollection().AddSingleton(
        std::make_shared<RateLimiter>(10, std::chrono::seconds(10)));

    auto app = builder.Build();

    app.MapGet("/", [](const HttpRequest& request, HttpResponse& response) {
        response.body = "<html><h1>Welcome to the Asio HTTP Server</h1></html>";
    });

    app.Use<LoggingMiddleware>();
    // app.Use<RateLimitMiddleware>();

    app.Run();

    return 0;
}
