#pragma once

#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <atomic>
#include <csignal>
#include <list>
#include <thread>
#include <utility>
#include <vector>

#include <Skirnir/Skirnir.hpp>

#include "AsioAdapter.hpp"
#include "HttpConnection.hpp"
#include "Net.hpp"

struct HttpServerOptions
{
    short port        = 8080;
    int   threadCount = std::thread::hardware_concurrency();
};

class HttpServer
{
  public:
    HttpServer(const skr::Arc<HttpServerOptions>&       httpServerOptions,
               const skr::Arc<skr::ServiceProvider>&    serviceProvider,
               const skr::Arc<skr::Logger<HttpServer>>& logger);

    skr::Task<> RunAsync();

    void Stop();

  private:
    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void onNewConnection();

    skr::Arc<skr::Logger<HttpServer>> mLogger;
    skr::Arc<skr::ServiceProvider>    mServiceProvider;
    skr::Arc<HttpServerOptions>       mHttpServerOptions;

    std::atomic<int>       mNextIoContext;
    net::io_context        mAcceptorIoContext;
    net::ip::tcp::acceptor mAcceptor;
    AsioScheduler          mScheduler;
};
