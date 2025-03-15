#pragma once

#include <iostream>
#include <memory>
#include <thread>
#include <utility>

#include <Skirnir/Skirnir.hpp>

#include "HttpSession.hpp"

class HttpServer
{
  public:
    HttpServer(const short                        port,
               const Ref<skr::ServiceCollection>& serviceCollection,
               const Ref<MiddlewareFactoryList>&  middlewareFactories,
               const Ref<PathMatcher>&            pathMatcher) :
        mIoContext(static_cast<int>(std::thread::hardware_concurrency())),
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
            std::function<void()> worker = [&] {
                try
                {
                    mIoContext.run();
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Thread exception: " << e.what() << "\n";
                    worker();
                }
            };
            threads.emplace_back(worker);
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
                }
                else
                {
                    std::cerr << "Error accepting connection: " << ec.message()
                              << std::endl;
                }
                acceptConnection();
            });
    }

    asio::io_context           mIoContext;
    asio::ip::tcp::acceptor    mAcceptor;
    Ref<skr::ServiceProvider>  mServiceProvider;
    Ref<MiddlewareFactoryList> mMiddlewareFactories;
    Ref<PathMatcher>           mPathMatcher;
};
