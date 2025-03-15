#include "Baldr/WebApplication.hpp"

#include <iostream>

#include "Baldr/HttpServer.hpp"
#include "Baldr/WebApplicationBuilder.hpp"

void WebApplication::Run() const
{
    try
    {
        mServiceCollection->AddTransient<skr::Logger<HttpServer>>();
        mServiceCollection->AddTransient<skr::Logger<HttpSession>>();

        auto server = HttpServer(
            8080, mServiceCollection, mMiddlewareFactories, mPathMatcher);

        server.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

WebApplicationBuilder WebApplication::CreateBuilder()
{
    return {};
}
