#pragma once

#include <atomic>
#include <memory>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/ServerOptions.hpp>
#include <Baldr/Application/InFlightTracker.hpp>

class HttpServer
{
  public:
    HttpServer(const skr::Arc<HttpServerOptions>&        httpServerOptions,
               const skr::Arc<skr::ServiceProvider>&     serviceProvider,
               const skr::Arc<skr::Logger<HttpServer>>&  logger,
               const skr::Arc<InFlightTracker>&          inFlightTracker);

    ~HttpServer();

    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void Run();

    void Stop();

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};