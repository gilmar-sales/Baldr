#include <Baldr/Http/Server.hpp>

#include <chrono>
#include <csignal>
#include <stdexcept>
#include <thread>

#include <trantor/net/TcpConnection.h>
#include <trantor/utils/MsgBuffer.h>

namespace
{
    HttpServer* gHttpServerInstance = nullptr;

    extern "C" void handleShutdownSignal(int signo)
    {
        if (gHttpServerInstance != nullptr)
        {
            // Async-signal-safe: only call Stop() which queues work on the
            // acceptor loop. Do not log or allocate from a signal handler.
            (void) signo;
            gHttpServerInstance->Stop();
        }
    }
} // namespace

int resolveThreadCount(int configured)
{
    return configured > 0
               ? configured
               : static_cast<int>(std::thread::hardware_concurrency());
}

HttpServer::HttpServer(const skr::Arc<HttpServerOptions>&       httpServerOptions,
                       const skr::Arc<skr::ServiceProvider>&    serviceProvider,
                       const skr::Arc<skr::Logger<HttpServer>>& logger,
                       const skr::Arc<InFlightTracker>&         inFlightTracker) :
    mServiceProvider(serviceProvider), mLogger(logger),
    mHttpServerOptions(httpServerOptions),
    mInFlightTracker(inFlightTracker),
    mResolvedThreadCount(
        std::max(1, resolveThreadCount(mHttpServerOptions->threadCount)))
{
}

HttpServer::~HttpServer()
{
    Stop();
}

void HttpServer::Run()
{
    if (mRunning.exchange(true))
        return;

    gHttpServerInstance = this;
    std::signal(SIGINT, handleShutdownSignal);
    std::signal(SIGTERM, handleShutdownSignal);

    auto middlewareProvider =
        mServiceProvider->GetService<MiddlewareProvider>();
    if (middlewareProvider && !middlewareProvider->IsSealed())
    {
        middlewareProvider->Seal();
        mLogger->LogInformation("MiddlewareProvider sealed with {} factories",
                                middlewareProvider->Size());
    }

    mAcceptorLoop = std::make_unique<trantor::EventLoop>();
    mIoLoopPool   = std::make_shared<trantor::EventLoopThreadPool>(
        static_cast<size_t>(mResolvedThreadCount));
    mIoLoopPool->start();

    trantor::InetAddress listenAddr(mHttpServerOptions->port);
    mServer = std::make_unique<trantor::TcpServer>(
        mAcceptorLoop.get(),
        listenAddr,
        "BaldrHttpServer",
        /*reUseAddr=*/true,
        /*reUsePort=*/false);
    mServer->setIoLoopNum(static_cast<size_t>(mResolvedThreadCount));

    auto serviceProvider = mServiceProvider;

    mServer->setConnectionCallback(
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

    mServer->setRecvMessageCallback(
        [](const trantor::TcpConnectionPtr& conn, trantor::MsgBuffer* buf) {
            auto handler = conn->getContext<HttpConnection>();
            if (!handler)
                return;
            handler->onMessage(buf);
        });

    mServer->start();

    mLogger->LogInformation(
        "Server running on http://0.0.0.0:{} with {} worker threads",
        mHttpServerOptions->port, mResolvedThreadCount);

    mAcceptorLoop->loop();

    // After acceptor loop exits, drain in-flight handlers before
    // tearing the IO loops down. The drain runs on the main thread;
    // the wait is bounded by `gracefulShutdownTimeoutSeconds`.
    int  timeoutSec = mHttpServerOptions->gracefulShutdownTimeoutSeconds;
    bool immediate = timeoutSec < 0;
    if (mInFlightTracker &&
        mInFlightTracker->outstanding() > 0 &&
        !immediate)
    {
        mLogger->LogInformation(
            "Draining {} in-flight requests (timeout {}s)...",
            mInFlightTracker->outstanding(), timeoutSec);
        mInFlightTracker->waitDrained(
            std::chrono::seconds(timeoutSec));
        mLogger->LogInformation(
            "Drained ({} in-flight remaining)",
            mInFlightTracker->outstanding());
    }

    mLogger->LogInformation("Server stopped.");
    mServer.reset();
    mIoLoopPool.reset();
    mAcceptorLoop.reset();
    mRunning.store(false);
    if (gHttpServerInstance == this)
    {
        gHttpServerInstance = nullptr;
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGTERM, SIG_DFL);
    }
}

void HttpServer::Stop()
{
    if (!mRunning.load())
        return;
    if (mAcceptorLoop)
    {
        mAcceptorLoop->runInLoop([this]() {
            if (mServer)
                mServer->stop();
            mAcceptorLoop->quit();
        });
    }
}
