#include <chrono>
#include <iostream>
#include <ranges>

#include "PathMatcher.hpp"
#include "WebApplication.hpp"
#include "WebApplicationBuilder.hpp"


class LoggingMiddleWare final : public IMiddleware {
public:
    void Handle(const HttpRequest &request, const HttpResponse &response) override {
        std::cout << "Request received: " << request.version << " " << request.method << " " << request.path <<
                std::endl;
    }
};

int main() {
    auto builder = WebApplication::CreateBuilder();

    auto app = builder.Build();

    app.MapGet("/", [](const HttpRequest &request,  HttpResponse &response) {
        response.body = "<html><h1>Welcome to the Asio HTTP Server</h1></html>";
    });

    app.Use<LoggingMiddleWare>();

    app.Run();

    return 0;
}
