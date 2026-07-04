#include <Baldr/Detail/Namespace.hpp>
#include <Baldr/Http/Server.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <stdexcept>
#include <thread>

#include <trantor/net/EventLoop.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <trantor/net/InetAddress.h>
#include <trantor/net/TcpConnection.h>
#include <trantor/net/TcpServer.h>
#include <trantor/utils/MsgBuffer.h>

#include <Baldr/Http/Connection.hpp>
#include <Baldr/Middleware/MiddlewareProvider.hpp>

namespace BALDR_NAMESPACE {

HttpServer* gHttpServerInstance = nullptr;

extern "C" void handleShutdownSignal(int signo)
{
    if (gHttpServerInstance != nullptr)
    {
        (void) signo;
        gHttpServerInstance->Stop();
    }
}

int resolveThreadCount(int configured)
{
    return configured > 0
               ? configured
               : static_cast<int>(std::thread::hardware_concurrency());
}

struct HttpServer::Impl
{
    skr::Arc<skr::Logger<HttpServer>> mLogger;
    skr::Arc<skr::ServiceProvider>    mServiceProvider;
    skr::Arc<HttpServerOptions>       mHttpServerOptions;
    skr::Arc<InFlightTracker>         mInFlightTracker;
    int                               mResolvedThreadCount { 1 };

    std::unique_ptr<trantor::EventLoop>           mAcceptorLoop;
    std::shared_ptr<trantor::EventLoopThreadPool> mIoLoopPool;
    std::unique_ptr<trantor::TcpServer>           mServer;
    std::atomic<bool>                             mRunning { false };
};

HttpServer::HttpServer(const skr::Arc<HttpServerOptions>&    httpServerOptions,
                       const skr::Arc<skr::ServiceProvider>& serviceProvider,
                       const skr::Arc<skr::Logger<HttpServer>>& logger,
                       const skr::Arc<InFlightTracker>& inFlightTracker) :
    mImpl(std::make_unique<Impl>())
{
    mImpl->mServiceProvider   = serviceProvider;
    mImpl->mLogger            = logger;
    mImpl->mHttpServerOptions = httpServerOptions;
    mImpl->mInFlightTracker   = inFlightTracker;
    mImpl->mResolvedThreadCount =
        std::max(1, resolveThreadCount(mImpl->mHttpServerOptions->threadCount));
}

HttpServer::~HttpServer()
{
    Stop();
}

void HttpServer::Run()
{
    if (mImpl->mRunning.exchange(true))
        return;

    gHttpServerInstance = this;
    std::signal(SIGINT, handleShutdownSignal);
    std::signal(SIGTERM, handleShutdownSignal);

    auto middlewareProvider =
        mImpl->mServiceProvider->GetService<MiddlewareProvider>();
    if (middlewareProvider && !middlewareProvider->IsSealed())
    {
        middlewareProvider->Seal();
        mImpl->mLogger->LogInformation(
            "MiddlewareProvider sealed with {} factories",
            middlewareProvider->Size());
    }

    mImpl->mAcceptorLoop = std::make_unique<trantor::EventLoop>();
    mImpl->mIoLoopPool   = std::make_shared<trantor::EventLoopThreadPool>(
        static_cast<size_t>(mImpl->mResolvedThreadCount));
    mImpl->mIoLoopPool->start();

    trantor::InetAddress listenAddr(mImpl->mHttpServerOptions->port);
    mImpl->mServer = std::make_unique<trantor::TcpServer>(
        mImpl->mAcceptorLoop.get(),
        listenAddr,
        "BaldrHttpServer",
        true,
        false);
    mImpl->mServer->setIoLoopNum(
        static_cast<size_t>(mImpl->mResolvedThreadCount));

    auto serviceProvider = mImpl->mServiceProvider;

    mImpl->mServer->setConnectionCallback(
        [serviceProvider](const trantor::TcpConnectionPtr& conn) {
            if (conn->connected())
            {
                auto handler =
                    std::make_shared<HttpConnection>(serviceProvider, conn);
                conn->setContext(handler);
            }
            else
            {
                conn->clearContext();
            }
        });

    mImpl->mServer->setRecvMessageCallback(
        [](const trantor::TcpConnectionPtr& conn, trantor::MsgBuffer* buf) {
            auto handler = conn->getContext<HttpConnection>();
            if (!handler)
                return;
            handler->onMessage(buf);
        });

    mImpl->mServer->start();

    mImpl->mLogger->LogInformation(
        "Server running on http://0.0.0.0:{} with {} worker threads",
        mImpl->mHttpServerOptions->port, mImpl->mResolvedThreadCount);

    mImpl->mAcceptorLoop->loop();

    int  timeoutSec = mImpl->mHttpServerOptions->gracefulShutdownTimeoutSeconds;
    bool immediate  = timeoutSec < 0;
    if (mImpl->mInFlightTracker && mImpl->mInFlightTracker->outstanding() > 0 &&
        !immediate)
    {
        mImpl->mLogger->LogInformation(
            "Draining {} in-flight requests (timeout {}s)...",
            mImpl->mInFlightTracker->outstanding(), timeoutSec);
        mImpl->mInFlightTracker->waitDrained(std::chrono::seconds(timeoutSec));
        mImpl->mLogger->LogInformation("Drained ({} in-flight remaining)",
                                       mImpl->mInFlightTracker->outstanding());
    }

    mImpl->mLogger->LogInformation("Server stopped.");
    mImpl->mServer.reset();
    mImpl->mIoLoopPool.reset();
    mImpl->mAcceptorLoop.reset();
    mImpl->mRunning.store(false);
    if (gHttpServerInstance == this)
    {
        gHttpServerInstance = nullptr;
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGTERM, SIG_DFL);
    }
}

void HttpServer::Stop()
{
    if (!mImpl->mRunning.load())
        return;
    if (mImpl->mAcceptorLoop)
    {
        mImpl->mAcceptorLoop->runInLoop([this]() {
            if (mImpl->mServer)
                mImpl->mServer->stop();
            mImpl->mAcceptorLoop->quit();
        });
    }
}

} // namespace BALDR_NAMESPACE