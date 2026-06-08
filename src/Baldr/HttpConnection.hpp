#pragma once

#include <asio/buffer.hpp>
#include <iostream>
#include <memory>

#include "AsioAwait.hpp"
#include "Baldr/MpMcPool.hpp"
#include "BufferPool.hpp"
#include "HttpRequestParser.hpp"
#include "HttpResponse.hpp"
#include "MiddlewareProvider.hpp"
#include "Net.hpp"
#include "Router.hpp"

#include <ranges>

class HttpConnection : public skr::enable_arc_from_this<HttpConnection>
{
    friend class HttpConnectionSpec;

  public:
    explicit HttpConnection(
        const skr::Arc<skr::ServiceProvider>& serviceProvider,
        net::ip::tcp::socket                  socket) :
        mServiceProvider(serviceProvider),
        mMiddlewareProvider(serviceProvider->GetService<MiddlewareProvider>()),
        mHttpRequestParser(serviceProvider->GetService<HttpRequestParser>()),
        mRouter(serviceProvider->GetService<Router>()),
        mSocket(std::move(socket))
    {
        mLogger = mServiceProvider->GetService<skr::Logger<HttpConnection>>();
        mBuffer = mServiceProvider->GetService<MpMcBufferPool>()->try_pop();
    }

    void start() { readRequest(); }

  private:
    skr::Task<> readRequest()
    {
        mBuffer->clear();
        std::size_t bytes_transferred = co_await baldr::AsioAwait<std::size_t>(
            mSocket.get_executor(),
            net::async_read_until(mSocket, net::dynamic_buffer(*mBuffer),
                                  "\r\n\r\n", net::use_awaitable));

        if (bytes_transferred > 0)
        {
            co_await processRequest(mBuffer, bytes_transferred);
        }

        co_return;
    }

    skr::Task<> processRequest(std::vector<char>* buffer,
                               std::size_t        bytes_transferred)
    {
        std::string request(buffer->data(), bytes_transferred);

        auto httpRequestParse = mHttpRequestParser->parse(request);

        if (!httpRequestParse.success)
        {
            mLogger->LogError("Failed to parse HTTP request: {}",
                              httpRequestParse.error);
            mBuffer->assign_range("HTTP/1.1 400 Bad Request\r\n\r\n");
            co_await writeResponse();
            co_return;
        }

        httpRequestParse.value.clientIp =
            mSocket.remote_endpoint().address().to_string();

        auto httpResponse = HttpResponse(httpRequestParse.value);

        auto current = mMiddlewareProvider->begin();

        const auto& routeEntry = mRouter->match(
            httpRequestParse.value.method, httpRequestParse.value.path);

        if (!routeEntry.has_value())
        {
            httpResponse.statusCode = StatusCode::NotFound;
            mBuffer->assign_range("HTTP/1.1 404 Not Found\r\n\r\n");
            co_await writeResponse();
            co_return;
        }

        httpRequestParse.value.params =
            routeEntry.value().extractRouteParams(httpRequestParse.value.path);

        NextMiddleware nextLambda = [&]() -> skr::Task<> {
            auto nextIt = current + 1;

            if (nextIt != mMiddlewareProvider->end())
            {
                current += 1;

                co_await (*nextIt)(mServiceProvider)
                    ->Handle(httpRequestParse.value, httpResponse, nextLambda);
            }

            co_await routeEntry.value().handler(
                httpRequestParse.value, httpResponse, mServiceProvider);

            if (!httpResponse.body.empty())
                httpResponse.headers["Content-Length"] =
                    std::to_string(httpResponse.body.size());

            co_return;
        };

        co_await (*current)(mServiceProvider)
            ->Handle(httpRequestParse.value, httpResponse, nextLambda);

        // Create the HTTP response
        std::ostringstream response_stream {};
        response_stream << httpResponse.version << " "
                        << httpResponse.statusCode << " OK\r\n";

        for (const auto& [headerName, headerValue] : httpResponse.headers)
        {
            response_stream << headerName << ": " << headerValue << "\r\n";
        }

        for (const auto& [cookieName, cookieOptions] : httpResponse.cookies)
        {
            response_stream
                << "Set-Cookie: " << cookieName << "=" << cookieOptions.value;

            switch (cookieOptions.sameSite)
            {
                case SameSite::None:
                    response_stream << "; SameSite=None";
                    break;
                case SameSite::Lax:
                    response_stream << "; SameSite=Lax";
                    break;
                case SameSite::Strict:
                    response_stream << "; SameSite=Strict";
                    break;
            }

            if (cookieOptions.domain.has_value())
                response_stream << "; Domain=" << cookieOptions.domain.value();

            if (cookieOptions.secure)
                response_stream << "; Secure";

            if (cookieOptions.httpOnly)
                response_stream << "; HttpOnly";

            if (cookieOptions.maxAge)
                response_stream << "; Max-Age=" << cookieOptions.maxAge;

            response_stream << "\r\n";
        }

        response_stream << "Connection: close\r\n\r\n" << httpResponse.body;

        mBuffer->clear();
        mBuffer->assign_range(response_stream.str());

        co_await writeResponse();
    }

    skr::Task<> writeResponse()
    {
        auto bytes_transferred = co_await baldr::AsioAwait<std::size_t>(
            mSocket.get_executor(),
            net::async_write(mSocket, net::buffer(*mBuffer),
                             net::use_awaitable));

        if (!mServiceProvider->GetService<MpMcBufferPool>()->try_push(mBuffer))
            delete mBuffer;

        mLogger->LogInformation("Message sent: {} bytes", bytes_transferred);

        co_return;
    }

    net::ip::tcp::socket mSocket;

    std::vector<char>* mBuffer;

    skr::Arc<skr::Logger<HttpConnection>> mLogger;
    skr::Arc<skr::ServiceProvider>        mServiceProvider;
    skr::Arc<HttpRequestParser>           mHttpRequestParser;
    skr::Arc<MiddlewareProvider>          mMiddlewareProvider;
    skr::Arc<Router>                      mRouter;
};
