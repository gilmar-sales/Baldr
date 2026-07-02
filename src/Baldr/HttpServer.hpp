#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <Skirnir/Skirnir.hpp>
#include <trantor/net/EventLoop.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <trantor/net/InetAddress.h>
#include <trantor/net/TcpServer.h>

#include "HttpConnection.hpp"

struct HttpServerOptions
{
    // NOTE: TLS is not currently supported. Baldr builds with
    // TRANTOR_USE_TLS=none, so https cannot be served. Adding TLS would
    // require either enabling OpenSSL in trantor (and wiring certificate
    // paths here) or terminating TLS upstream (e.g. via a reverse proxy).
    short port        = 8080;
    int   threadCount = static_cast<int>(std::thread::hardware_concurrency());
};

class HttpServer
{
  public:
    HttpServer(const skr::Arc<HttpServerOptions>&       httpServerOptions,
               const skr::Arc<skr::ServiceProvider>&    serviceProvider,
               const skr::Arc<skr::Logger<HttpServer>>& logger);

    ~HttpServer();

    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void Run();

    void Stop();

  private:
    skr::Arc<skr::Logger<HttpServer>> mLogger;
    skr::Arc<skr::ServiceProvider>    mServiceProvider;
    skr::Arc<HttpServerOptions>       mHttpServerOptions;
    int                               mResolvedThreadCount;

    std::unique_ptr<trantor::EventLoop>           mAcceptorLoop;
    std::shared_ptr<trantor::EventLoopThreadPool> mIoLoopPool;
    std::unique_ptr<trantor::TcpServer>           mServer;
    std::atomic<bool>                             mRunning { false };
};
