#pragma once

#include <iostream>
#include <list>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <Skirnir/Skirnir.hpp>

#include "HttpConnection.hpp"

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
        mAcceptor(addIoContext())
    {
        asio::ip::tcp::resolver resolver(mAcceptor.get_executor());
        asio::ip::tcp::endpoint endpoint =
            *resolver.resolve("0.0.0.0", "8080").begin();
        mAcceptor.open(endpoint.protocol());
        mAcceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
        mAcceptor.bind(endpoint);
        mAcceptor.listen();

        for (auto i = 0; i < httpServerOptions->threadCount; i++)
        {
            addIoContext();
        }

        onNewConnection();
    }

    void Run()
    {
        std::vector<std::jthread> threads;

        for (std::size_t i = 0; i < mIoContexts.size(); ++i)
        {
            std::function<void()> worker = [this, i, &worker] {
                try
                {
                    mIoContexts[i]->run();
                }
                catch (const std::exception& e)
                {
                    mLogger->LogError("Thread exception: {}", e.what());
                    worker();
                }
            };
            threads.emplace_back(worker);
        }

        mLogger->LogInformation("🚀 Server running on http://localhost:{}",
                                mHttpServerOptions->port);
    }

    void Stop()
    {
        for (auto& ioContext : mIoContexts)
        {
            ioContext->stop();
        }

        mLogger->LogInformation("🛑 Server stopped.");
    }

  private:
    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    asio::io_context& addIoContext()
    {
        auto ioContext = skr::MakeRef<asio::io_context>();
        mIoContexts.push_back(ioContext);
        mWorkGuards.push_back(asio::make_work_guard(*ioContext));

        return *ioContext;
    }

    asio::io_context& getIoContext()
    {
        auto& ioContext = *mIoContexts[mNextIoContext];

        ++mNextIoContext;

        if (mNextIoContext == mIoContexts.size())
            mNextIoContext = 0;

        return ioContext;
    }

    void onNewConnection()
    {
        mAcceptor.async_accept(
            getIoContext(),
            [this](const std::error_code ec, asio::ip::tcp::socket socket) {
                if (!mAcceptor.is_open())
                {
                    return;
                }

                if (!ec)
                {
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

    std::list<asio::executor_work_guard<asio::io_context::executor_type>>
                                       mWorkGuards;
    std::vector<Ref<asio::io_context>> mIoContexts;
    size_t                             mNextIoContext;
    asio::ip::tcp::acceptor            mAcceptor;
};
