#include <chrono>
#include <iostream>

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

struct Person
{
    std::string name;
    int age;
};

void to_json(nlohmann::json& j, const Person& p) {
    j = nlohmann::json{{"name", p.name}, {"age", p.age}};
}

void from_json(const nlohmann::json& j, Person& p) {
    j.at("name").get_to(p.name);
    j.at("age").get_to(p.age);
}

int main()
{
    auto builder = WebApplication::CreateBuilder();

    builder.GetServiceCollection().AddSingleton(
        std::make_shared<RateLimiter>(10, std::chrono::seconds(10)));

    auto app = builder.Build();

    app.MapPost("/", [](const HttpRequest& request, HttpResponse& response,
                       std::shared_ptr<RateLimiter> rate_limiter) {
        std::cout << rate_limiter->isAllowed("/") << std::endl;

        response.body = "<html><h1>Welcome to the Asio HTTP Server</h1></html>";
        response.statusCode = StatusCode::OK;

        return Person{.name = "Gilmar", .age = 25};
    });

    app.Use<LoggingMiddleware>();
    // app.Use<RateLimitMiddleware>();

    app.Run();

    return 0;
}
