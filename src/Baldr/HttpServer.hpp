#pragma once

#include <atomic>
#include <csignal>
#include <thread>
#include <vector>

#include <Skirnir/Skirnir.hpp>
#include <h2o.h>

#include "HttpConnection.hpp"

struct HttpServerOptions
{
    short port        = 8080;
    int   threadCount = std::thread::hardware_concurrency();
};

class HttpServer
{
  public:
    struct WorkerContext
    {
        uv_loop_t        loop;
        h2o_context_t    h2oCtx;
        h2o_accept_ctx_t acceptCtx;
        uv_tcp_t         listener;
        std::thread      thread;
    };

  public:
    HttpServer(const skr::Arc<HttpServerOptions>&       httpServerOptions,
               const skr::Arc<skr::ServiceProvider>&    serviceProvider,
               const skr::Arc<skr::Logger<HttpServer>>& logger);

    ~HttpServer();

    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    skr::Task<> RunAsync();

    void Stop();

  private:
    void workerLoop(WorkerContext& ctx);

    skr::Arc<skr::Logger<HttpServer>> mLogger;
    skr::Arc<skr::ServiceProvider>    mServiceProvider;
    skr::Arc<HttpServerOptions>       mHttpServerOptions;
    int                               mResolvedThreadCount;

    h2o_globalconf_t                mGlobalConfig;
    h2o_hostconf_t*                 mHostConfig = nullptr;
    h2o_pathconf_t*                 mPathConfig = nullptr;
    h2o_handler_t*                  mRootHandler = nullptr;
    HttpConnection*                 mConnection  = nullptr;
    std::vector<std::unique_ptr<WorkerContext>> mWorkers;
    std::atomic<bool>               mRunning { false };
};
