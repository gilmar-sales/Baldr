#include "HttpServer.hpp"

HttpServer::HttpServer(const skr::Arc<HttpServerOptions>&    httpServerOptions,
                       const skr::Arc<skr::ServiceProvider>& serviceProvider,
                       const skr::Arc<skr::Logger<HttpServer>>& logger) :
    mServiceProvider(serviceProvider), mLogger(logger),
    mHttpServerOptions(httpServerOptions),
    mThreadPool(
        static_cast<std::size_t>(mHttpServerOptions->threadCount)),
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
    mAcceptor.listen();
}

skr::Task<> HttpServer::RunAsync()
{
    net::signal_set signals(mThreadPool, SIGINT, SIGTERM);
    signals.async_wait([this](net::error_code, int) { Stop(); });

    std::vector<std::jthread> threads;

    for (std::size_t i = 0; i < mHttpServerOptions->threadCount; ++i)
    {
        std::function<void()> worker = [this, i, &worker] {
            try
            {
                onNewConnection();

                mThreadPool.join();
            }
            catch (const std::exception& e)
            {
                mLogger->LogError("Worker thread {} exception: {}", i,
                                  e.what());
                worker();
            }
        };
        threads.emplace_back(worker);
    }

    mLogger->LogInformation(
        "🚀 Server running on http://localhost:{} with {} worker threads",
        mHttpServerOptions->port, mHttpServerOptions->threadCount);

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
                    net::socket_base::send_buffer_size(256 * 1024));
                socket.set_option(
                    net::socket_base::receive_buffer_size(256 * 1024));
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
