#include <chrono>

#include "RateLimitMiddleware.h"
#include "WebApplication.hpp"
#include "WebApplicationBuilder.hpp"

struct Person
{
    std::string name;
    int         age;
};

int main()
{
    auto builder = WebApplication::CreateBuilder();

    builder.GetServiceCollection().AddSingleton(
        std::make_shared<RateLimiter>(10, std::chrono::seconds(10)));

    auto app = builder.Build();

    app.MapGet("/", [](HttpResponse& response) {
        response.body       = "Hello, World!";
        response.statusCode = StatusCode::OK;

        return std::vector { Person { .name = "Gilmar", .age = 25 },
                             Person { .name = "Rayssa", .age = 19 } };
    });

    app.Run();

    return 0;
}
