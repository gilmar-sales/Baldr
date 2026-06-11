#include "HttpServer.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <h2o.h>
#include <uv.h>

#include "Skirnir/Async/Task.hpp"

int resolveThreadCount(int configured)
{
    return configured > 0
               ? configured
               : static_cast<int>(std::thread::hardware_concurrency());
}

namespace
{
    void onUvClose(uv_handle_t* handle)
    {
        delete handle;
    }

    void onAccept(uv_stream_t* listener, int status)
    {
        auto* ctx = reinterpret_cast<HttpServer::WorkerContext*>(listener->data);
        if (status != 0)
            return;

        auto* conn = new uv_tcp_t();
        if (uv_tcp_init(&ctx->loop, conn) != 0)
        {
            delete conn;
            return;
        }
        if (uv_accept(listener, reinterpret_cast<uv_stream_t*>(conn)) != 0)
        {
            uv_close(reinterpret_cast<uv_handle_t*>(conn), onUvClose);
            return;
        }

        h2o_socket_t* sock = h2o_uv_socket_create(
            reinterpret_cast<uv_handle_t*>(conn), onUvClose);
        h2o_accept(&ctx->acceptCtx, sock);
    }
}

HttpServer::HttpServer(const skr::Arc<HttpServerOptions>&    httpServerOptions,
                       const skr::Arc<skr::ServiceProvider>& serviceProvider,
                       const skr::Arc<skr::Logger<HttpServer>>& logger) :
    mServiceProvider(serviceProvider),
    mLogger(logger),
    mHttpServerOptions(httpServerOptions),
    mResolvedThreadCount(
        std::max(1, resolveThreadCount(mHttpServerOptions->threadCount)))
{
    h2o_config_init(&mGlobalConfig);

    mHostConfig = h2o_config_register_host(
        &mGlobalConfig,
        h2o_iovec_init(H2O_STRLIT("default")), 65535);
    mPathConfig = h2o_config_register_path(mHostConfig, "/", 0);

    mConnection = new HttpConnection(mServiceProvider);
    mRootHandler =
        h2o_create_handler(mPathConfig, sizeof(HttpConnection::H2oHandler));
    mRootHandler->on_req = HttpConnection::onRequest;
    reinterpret_cast<HttpConnection::H2oHandler*>(mRootHandler)->connection =
        mConnection;
}

HttpServer::~HttpServer()
{
    Stop();
    delete mConnection;
    mConnection  = nullptr;
    mRootHandler = nullptr;
    h2o_config_dispose(&mGlobalConfig);
}

skr::Task<> HttpServer::RunAsync()
{
    if (mRunning.exchange(true))
        co_return;

    for (int i = 0; i < mResolvedThreadCount; ++i)
    {
        mLogger->LogInformation("Starting worker {}", i);
        auto ctx  = std::make_unique<WorkerContext>();
        uv_loop_t* loop = &ctx->loop;
        if (uv_loop_init(loop) != 0)
            throw std::runtime_error("uv_loop_init failed");
        h2o_context_init(&ctx->h2oCtx, loop, &mGlobalConfig);

        ctx->acceptCtx.ctx   = &ctx->h2oCtx;
        ctx->acceptCtx.hosts = mGlobalConfig.hosts;

        uv_tcp_t& listener = ctx->listener;
        uv_tcp_init(loop, &listener);
        listener.data = ctx.get();

        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons(mHttpServerOptions->port);

        if (uv_tcp_bind(&listener, reinterpret_cast<sockaddr*>(&addr),
                        UV_TCP_REUSEPORT) != 0)
        {
            throw std::runtime_error("uv_tcp_bind failed");
        }
        if (uv_listen(reinterpret_cast<uv_stream_t*>(&listener), 4096,
                      onAccept) != 0)
        {
            throw std::runtime_error("uv_listen failed");
        }

        ctx->thread = std::thread([this, rawCtx = ctx.get()]() {
            workerLoop(*rawCtx);
        });

        mWorkers.push_back(std::move(ctx));
    }

    mLogger->LogInformation(
        "Server running on http://0.0.0.0:{} with {} worker threads",
        mHttpServerOptions->port, mResolvedThreadCount);

    for (auto& ctx : mWorkers)
    {
        if (ctx->thread.joinable())
            ctx->thread.join();
    }

    mLogger->LogInformation("Server stopped.");
    co_return;
}

void HttpServer::Stop()
{
    if (!mRunning.exchange(false))
        return;
    for (auto& ctx : mWorkers)
    {
        uv_stop(&ctx->loop);
    }
    for (auto& ctx : mWorkers)
    {
        if (ctx->thread.joinable())
            ctx->thread.join();
    }
    mWorkers.clear();
}

void HttpServer::workerLoop(WorkerContext& ctx)
{
    uv_run(&ctx.loop, UV_RUN_DEFAULT);
    h2o_context_dispose(&ctx.h2oCtx);
    uv_loop_close(&ctx.loop);
}
