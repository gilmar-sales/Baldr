#include "HttpServer.hpp"

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <cstdlib>
#include <string>

int resolveThreadCount(int configured)
{

    return configured > 0
               ? configured
               : static_cast<int>(std::thread::hardware_concurrency());
}

HttpServer::HttpServer(const skr::Arc<HttpServerOptions>&    httpServerOptions,
                       const skr::Arc<skr::ServiceProvider>& serviceProvider,
                       const skr::Arc<skr::Logger<HttpServer>>& logger) :
    mServiceProvider(serviceProvider), mLogger(logger),
    mHttpServerOptions(httpServerOptions),
    mResolvedThreadCount(resolveThreadCount(mHttpServerOptions->threadCount)),
    mThreadPool(static_cast<std::size_t>(mResolvedThreadCount)),
    mAcceptor(mThreadPool.get_executor()),
    mScheduler(mThreadPool.get_executor())
{
    net::ip::tcp::resolver resolver(mThreadPool);
    net::ip::tcp::endpoint endpoint =
        *resolver.resolve("0.0.0.0", std::to_string(mHttpServerOptions->port))
             .begin();
    mAcceptor.open(endpoint.protocol());
    mAcceptor.set_option(net::ip::tcp::acceptor::reuse_address(true));
    mAcceptor.bind(endpoint);
    mAcceptor.listen(4096);
}

skr::Task<> HttpServer::RunAsync()
{
    net::signal_set signals(mThreadPool, SIGINT, SIGTERM);
    signals.async_wait([this](net::error_code, int) { Stop(); });

    net::co_spawn(mThreadPool, listener(), net::detached);

    mLogger->LogInformation(
        "🚀 Server running on http://localhost:{} with {} worker threads",
        mHttpServerOptions->port, mResolvedThreadCount);

    mThreadPool.join();
    mLogger->LogInformation("🛑 Server stopped.");
    co_return;
}

void HttpServer::Stop()
{
    mLogger->LogInformation("🛑 Server stopping.");
    mThreadPool.stop();
}

net::awaitable<void> HttpServer::listener()
{
    for (;;)
    {
        auto socket = co_await mAcceptor.async_accept(net::use_awaitable);
        auto strand = asio::make_strand(mThreadPool);
        net::co_spawn(strand, onNewConnection(std::move(socket)),
                      net::detached);
    }
}

net::awaitable<void> HttpServer::onNewConnection(net::ip::tcp::socket socket)
{
    mLogger->LogDebug("New connection");
    socket.set_option(net::socket_base::send_buffer_size(32 * 1024));
    socket.set_option(net::socket_base::receive_buffer_size(32 * 1024));
    socket.set_option(net::ip::tcp::no_delay(true));
    socket.set_option(net::socket_base::keep_alive(true));

    auto httpConnection = skr::MakeArc<HttpConnection>(mServiceProvider,
                                                    std::move(socket));
                                                    
    net::post(mThreadPool, [httpConnection]() { httpConnection->start(); });

    co_return;
}
