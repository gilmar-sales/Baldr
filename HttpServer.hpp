#pragma once

#include <iostream>
#include <memory>
#include <asio.hpp>
#include <ServiceScope.hpp>
#include <utility>

#include "HttpSession.hpp"


class HttpServer {
public:
    HttpServer(const short port,
               const std::shared_ptr<ServiceCollection> &serviceCollection,
               const std::shared_ptr<MiddlewareFactoryList> &middlewareFactories,
               const std::shared_ptr<PathMatcher> &pathMatcher)
        : mIoContext({}),
          mAcceptor(mIoContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
          mServiceProvider(serviceCollection->CreateServiceProvider()),
          mMiddlewareFactories(middlewareFactories),
          mPathMatcher(pathMatcher) {
        acceptConnection();
        std::cout << "HttpServer running on http://localhost:" << port << std::endl;
    }

    void Run() {
        mIoContext.run();
    }

private:
    void acceptConnection() {
        mAcceptor.async_accept(
            [this](const std::error_code ec, asio::ip::tcp::socket socket) {
                if (!ec) {
                    const auto scope = mServiceProvider->CreateServiceScope();

                    std::make_shared<HttpSession>(std::move(socket), scope->GetServiceProvider(), mMiddlewareFactories,
                                                  mPathMatcher)->start();
                } else {
                    std::cerr << "Error accepting connection: " << ec.message() << std::endl;
                }
                acceptConnection();
            });
    }


    asio::io_context mIoContext;
    asio::ip::tcp::acceptor mAcceptor;
    std::shared_ptr<ServiceProvider> mServiceProvider;
    std::shared_ptr<MiddlewareFactoryList> mMiddlewareFactories;
    std::shared_ptr<PathMatcher> mPathMatcher;
};
