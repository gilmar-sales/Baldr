#include "Baldr/WebApplication.hpp"

#include <iostream>

#include "Baldr/HttpServer.hpp"
#include "Baldr/WebApplicationBuilder.hpp"

void WebApplication::Run() const
{
    mServiceCollection->AddSingleton<HttpServer>();
    mServiceCollection->AddTransient<skr::Logger<HttpSession>>();

    auto serviceProvider = mServiceCollection->CreateServiceProvider();

    auto logger = serviceProvider->GetService<skr::Logger<WebApplication>>();

    try
    {
        auto server = serviceProvider->GetService<HttpServer>();

        server->Run();
    }
    catch (const std::exception& e)
    {
        logger->LogError("{}", e.what());
    }
}

WebApplicationBuilder WebApplication::CreateBuilder()
{
    return {};
}
