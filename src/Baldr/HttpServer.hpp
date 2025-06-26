#pragma once

#include <iostream>
#include <memory>
#include <thread>
#include <utility>

#include <Skirnir/Skirnir.hpp>

#include "HttpSession.hpp"

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
        mHttpServerOptions(httpServerOptions),
        mIoContext(httpServerOptions->threadCount),
        mAcceptor(mIoContext,
                  asio::ip::tcp::endpoint(
                      asio::ip::tcp::v4(), httpServerOptions->port))
    {
        mAcceptor.set_option(asio::socket_base::reuse_address(true));
        mAcceptor.listen(asio::socket_base::max_listen_connections);

        acceptConnection();

        mLogger->LogInformation("🚀 Server running on http://localhost:{}",
                                httpServerOptions->port);
    }

    void Run()
    {
        std::vector<std::jthread> threads;

        for (std::size_t i = 0; i < mHttpServerOptions->threadCount; ++i)
        {
            std::function<void()> worker = [&] {
                try
                {
                    mIoContext.run();
                }
                catch (const std::exception& e)
                {
                    mLogger->LogError("Thread exception: {}", e.what());
                    worker();
                }
            };
            threads.emplace_back(worker);
        }
    }

  private:
    void acceptConnection()
    {
        mAcceptor.async_accept(
            [this](const std::error_code ec, asio::ip::tcp::socket socket) {
                if (!ec)
                {
                    socket.set_option(asio::ip::tcp::no_delay(true));

                    const auto scope = mServiceProvider->CreateServiceScope();

                    auto httpSession = skr::MakeRef<HttpSession>(
                        scope->GetServiceProvider(),
                        scope->GetServiceProvider()
                            ->GetService<MiddlewareProvider>(),
                        scope->GetServiceProvider()->GetService<Router>(),
                        std::move(socket));

                    httpSession->start();
                }
                else
                {
                    mLogger->LogError("Error accepting conenction: {}",
                                      ec.message());
                }

                acceptConnection();
            });
    }

    Ref<skr::Logger<HttpServer>> mLogger;
    Ref<skr::ServiceProvider>    mServiceProvider;
    Ref<HttpServerOptions>       mHttpServerOptions;

    asio::io_context        mIoContext;
    asio::ip::tcp::acceptor mAcceptor;
};
