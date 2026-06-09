#include "HttpServer.hpp"

#include <cstdlib>
#include <string>

int resolveThreadCount(int configured)
{

    return configured > 0 ? configured
                          : static_cast<int>(std::thread::hardware_concurrency());
}

HttpServer::HttpServer(const skr::Arc<HttpServerOptions>&    httpServerOptions,
                       const skr::Arc<skr::ServiceProvider>& serviceProvider,
                       const skr::Arc<skr::Logger<HttpServer>>& logger) :
    mServiceProvider(serviceProvider), mLogger(logger),
    mHttpServerOptions(httpServerOptions),
    mResolvedThreadCount(
        resolveThreadCount(mHttpServerOptions->threadCount)),
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

    onNewConnection();

    std::vector<std::jthread> threads;
    const auto workerCount = mResolvedThreadCount;

    for (int i = 0; i < workerCount; ++i)
    {
        std::jthread worker([this, i] {
            try
            {
                mThreadPool.join();
            }
            catch (const std::exception& e)
            {
                mLogger->LogError("Worker thread {} exception: {}", i,
                                  e.what());
            }
        });
        threads.push_back(std::move(worker));
    }

    mLogger->LogInformation(
        "🚀 Server running on http://localhost:{} with {} worker threads",
        mHttpServerOptions->port, workerCount);

    co_return;
}

void HttpServer::Stop()
{
    mThreadPool.stop();
    mLogger->LogInformation("🛑 Server stopped.");
}

void HttpServer::onNewConnection()
{
    mAcceptor.async_accept(
        mThreadPool,
        [this](const std::error_code ec, net::ip::tcp::socket socket) {
            if (!mAcceptor.is_open())
            {
                return;
            }

            if (!ec)
            {
                socket.set_option(
                    net::socket_base::send_buffer_size(32 * 1024));
                socket.set_option(
                    net::socket_base::receive_buffer_size(32 * 1024));
                socket.set_option(net::ip::tcp::no_delay(true));
                socket.set_option(net::socket_base::keep_alive(true));

                const auto scope = mServiceProvider->CreateServiceScope();

                auto httpSession =
                    skr::MakeArc<HttpConnection>(scope->GetServiceProvider(),
                                                 std::move(socket));

                httpSession->start();
            }
            else
            {
                mLogger->LogError("Error accepting connection: {}",
                                  ec.message());
            }

            onNewConnection();
        });
}
