#pragma once

#include <iostream>
#include <memory>
#include <thread>
#include <utility>

#include <ServiceScope.hpp>
#include <asio.hpp>

#include "HttpSession.hpp"

class HttpServer
{
  public:
    HttpServer(
        const short                                   port,
        const std::shared_ptr<ServiceCollection>&     serviceCollection,
        const std::shared_ptr<MiddlewareFactoryList>& middlewareFactories,
        const std::shared_ptr<PathMatcher>&           pathMatcher) :
        mIoContext(std::thread::hardware_concurrency()),
        mAcceptor(mIoContext,
                  asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
        mServiceProvider(serviceCollection->CreateServiceProvider()),
        mMiddlewareFactories(middlewareFactories), mPathMatcher(pathMatcher)
    {
        acceptConnection();
        std::cout << "HttpServer running on http://localhost:" << port
                  << std::endl;
        mAcceptor.set_option(asio::socket_base::reuse_address(true));
        mAcceptor.listen(asio::socket_base::max_listen_connections);
    }

    void Run()
    {
        // Create a thread pool
        std::vector<std::thread> threads;
        for (std::size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
        {
            threads.emplace_back([this]() {
                try
                {
                    mIoContext.run();
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Thread exception: " << e.what() << "\n";
                }
            });
        }

        // Join threads
        for (auto& thread : threads)
        {
            thread.join();
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

                    std::make_shared<HttpSession>(
                        std::move(socket), scope->GetServiceProvider(),
                        mMiddlewareFactories, mPathMatcher)
                        ->start();

                    socket.release();
                }
                else
                {
                    std::cerr << "Error accepting connection: " << ec.message()
                              << std::endl;
                }
                acceptConnection();
            });
    }

    asio::io_context                       mIoContext;
    asio::ip::tcp::acceptor                mAcceptor;
    std::shared_ptr<ServiceProvider>       mServiceProvider;
    std::shared_ptr<MiddlewareFactoryList> mMiddlewareFactories;
    std::shared_ptr<PathMatcher>           mPathMatcher;
};
