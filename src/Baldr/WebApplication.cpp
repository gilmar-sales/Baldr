#include "Baldr/WebApplication.hpp"

#include <iostream>

#include "Baldr/HttpServer.hpp"

void WebApplication::Run()
{
    auto logger =
        mRootServiceProvider->GetService<skr::Logger<WebApplication>>();

    try
    {
        auto server = mRootServiceProvider->GetService<HttpServer>();

        server->Run();
    }
    catch (const std::exception& e)
    {
        logger->LogError("{}", e.what());
    }
}