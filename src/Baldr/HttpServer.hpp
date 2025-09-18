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
    HttpServer(const Ref<HttpServerOptions>&       httpServerOptions,
               const Ref<skr::ServiceProvider>&    serviceProvider,
               const Ref<skr::Logger<HttpServer>>& logger) :
        mServiceProvider(serviceProvider), mLogger(logger),
        mHttpServerOptions(httpServerOptions), mNextIoContext(0),
        mAcceptorIoContext(mHttpServerOptions->threadCount),
        mAcceptor(mAcceptorIoContext)
    {
        net::ip::tcp::resolver resolver(mAcceptorIoContext);
        net::ip::tcp::endpoint endpoint =
            *resolver
                 .resolve("0.0.0.0", std::to_string(mHttpServerOptions->port))
                 .begin();
        mAcceptor.open(endpoint.protocol());
        mAcceptor.set_option(net::ip::tcp::acceptor::reuse_address(true));
        mAcceptor.bind(endpoint);
        mAcceptor.listen();
    }

    void Run()
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
    }

    void Stop()
    {
        mAcceptorIoContext.stop();
        mLogger->LogInformation("🛑 Server stopped.");
    }

  private:
    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void addIoContext()
    {
        mIoContexts.push_back(std::move(new net::io_context()));
        mWorkGuards.push_back(net::make_work_guard(*mIoContexts.back()));
    }

    net::io_context& getIoContext()
    {
        auto next = mNextIoContext.fetch_add(1) % mIoContexts.size();

        return *mIoContexts[next];
    }

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

                    auto httpSession = skr::MakeRef<HttpConnection>(
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

    Ref<skr::Logger<HttpServer>> mLogger;
    Ref<skr::ServiceProvider>    mServiceProvider;
    Ref<HttpServerOptions>       mHttpServerOptions;

    std::list<net::executor_work_guard<net::io_context::executor_type>>
                                  mWorkGuards;
    std::vector<net::io_context*> mIoContexts;
    std::atomic<int>              mNextIoContext;
    net::io_context               mAcceptorIoContext;
    net::ip::tcp::acceptor        mAcceptor;
};
