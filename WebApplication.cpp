#include "WebApplication.hpp"

#include <iostream>

#include "HttpServer.hpp"
#include "WebApplicationBuilder.hpp"

void WebApplication::Run() const {
    try {
        auto server = HttpServer(8080, mServiceCollection, mMiddlewareFactories, mPathMatcher);

        server.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

WebApplicationBuilder WebApplication::CreateBuilder() {
    return {};
}
