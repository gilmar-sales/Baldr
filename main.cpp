#include <chrono>
#include <iostream>

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
    auto pathMatch = PathMatcher();

    pathMatch.insert({"/"});
    pathMatch.insert({"hello"});
    pathMatch.insert({"user", "*"});

    auto begin = std::chrono::high_resolution_clock::now();
    auto a = pathMatch.match({"/"});
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - begin) << std::endl;

    begin = std::chrono::high_resolution_clock::now();
    auto b = pathMatch.match({"user", "123"});
    end = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - begin) << std::endl;


    auto builder = WebApplication::CreateBuilder();

    auto app = builder.Build();

    app.Use<LoggingMiddleWare>();

    app.Run();

    return 0;
}
