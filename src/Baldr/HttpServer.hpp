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
               const skr::Arc<skr::Logger<HttpServer>>& logger) :
        mServiceProvider(serviceProvider), mLogger(logger),
        mHttpServerOptions(httpServerOptions),
        mIoContext(mHttpServerOptions->threadCount), mAcceptor(mIoContext),
        mScheduler(mIoContext.get_executor())
    {
        net::ip::tcp::resolver resolver(mIoContext);
        net::ip::tcp::endpoint endpoint =
            *resolver
                 .resolve("0.0.0.0", std::to_string(mHttpServerOptions->port))
                 .begin();
        mAcceptor.open(endpoint.protocol());
        mAcceptor.set_option(net::ip::tcp::acceptor::reuse_address(true));
        mAcceptor.bind(endpoint);
        mAcceptor.listen();
    }

    skr::Task<> RunAsync()
    {
        onNewConnection();

        net::signal_set signals(mAcceptorIoContext, SIGINT, SIGTERM);
        signals.async_wait([this](net::error_code, int) { Stop(); });

        std::vector<std::jthread> threads;

        // Start worker threads
        for (std::size_t i = 0; i < mHttpServerOptions->threadCount; ++i)
        {
            std::function<void()> worker = [this, i, &worker] {
                try
                {
                    mAcceptorIoContext.run();
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

    void Stop()
    {
        mAcceptorIoContext.stop();
        mLogger->LogInformation("🛑 Server stopped.");
    }

  private:
    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void onNewConnection()
    {
        mAcceptor.async_accept(
            mAcceptorIoContext,
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

                    auto httpSession = skr::MakeArc<HttpConnection>(
                        scope->GetServiceProvider(),
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

    skr::Arc<skr::Logger<HttpServer>> mLogger;
    skr::Arc<skr::ServiceProvider>    mServiceProvider;
    skr::Arc<HttpServerOptions>       mHttpServerOptions;

    std::list<net::executor_work_guard<net::io_context::executor_type>>
                           mWorkGuards;
    std::atomic<int>       mNextIoContext;
    net::io_context        mIoContext;
    net::ip::tcp::acceptor mAcceptor;
    AsioScheduler          mScheduler;
};
