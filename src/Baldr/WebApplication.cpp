#include "Baldr/WebApplication.hpp"

#include <iostream>

#include "Baldr/HttpServer.hpp"

skr::Task<> WebApplication::RunAsync()
{
    auto logger = co_await mRootServiceProvider
                      ->GetServiceAsync<skr::Logger<WebApplication>>();
    try
    {
        auto server = co_await mRootServiceProvider->GetServiceAsync<HttpServer>();

        co_await server->RunAsync();
    }
    catch (const std::exception& e)
    {
        logger->LogError("{}", e.what());
    }
}