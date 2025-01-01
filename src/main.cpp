#include <chrono>

#include "RateLimitMiddleware.hpp"
#include "WebApplication.hpp"
#include "WebApplicationBuilder.hpp"

struct Person
{
    std::string name;
    int         age;
};

int main()
{
    auto builder = WebApplication::CreateBuilder() | AddRateLimit();

    auto app = builder.Build();

    app.MapGet("/", [](HttpResponse& response) {
        response.body       = "Hello, World!";
        response.statusCode = StatusCode::OK;

        return Person { .name = "Gilmar", .age = 25 };
    });
    app.Use<RateLimitMiddleware>();

    app.Run();

    return 0;
}
