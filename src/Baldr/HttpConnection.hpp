#pragma once

#include <asio/buffer.hpp>
#include <iostream>
#include <memory>

#include <rfl/enums.hpp>

#include "Baldr/MpMcPool.hpp"
#include "BufferPool.hpp"
#include "HttpRequestParser.hpp"
#include "HttpResponse.hpp"
#include "MiddlewareProvider.hpp"
#include "Net.hpp"
#include "Router.hpp"

#include <ranges>

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
    friend class HttpConnectionSpec;

  public:
    explicit HttpConnection(const Ref<skr::ServiceProvider>& serviceProvider,
                            net::ip::tcp::socket             socket) :
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
    void readRequest()
    {
        auto self = shared_from_this();
        async_read_until(
            mSocket, net::dynamic_buffer(*mBuffer), "\r\n\r\n",
            [self](const std::error_code ec,
                   const std::size_t     bytes_transferred) {
                if (!ec)
                {
                    self->processRequest(self->mBuffer, bytes_transferred);
                }
            });
    }

    void processRequest(std::vector<char>* buffer,
                        std::size_t        bytes_transferred)
    {
        std::string request(buffer->data(), bytes_transferred);

        auto httpRequestParse = mHttpRequestParser->parse(request);

        if (!httpRequestParse.success)
        {
            mLogger->LogError("Failed to parse HTTP request: {}",
                              httpRequestParse.error);
            mBuffer->assign_range("HTTP/1.1 400 Bad Request\r\n\r\n");
            writeResponse();
            return;
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
            return writeResponse();
        }

        httpRequestParse.value.params =
            routeEntry.value().extractRouteParams(httpRequestParse.value.path);

        NextMiddleware nextLambda = [&]() -> void {
            auto nextIt = current + 1;

            if (nextIt != mMiddlewareProvider->end())
            {
                current += 1;

                return (*nextIt)(mServiceProvider)
                    ->Handle(httpRequestParse.value, httpResponse, nextLambda);
            }

            routeEntry.value().handler(
                httpRequestParse.value, httpResponse, mServiceProvider);

            if (!httpResponse.body.empty())
                httpResponse.headers["Content-Length"] =
                    std::to_string(httpResponse.body.size());
        };

        (*current)(mServiceProvider)
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
        writeResponse();
    }

    void writeResponse()
    {
        net::async_write(
            mSocket, net::buffer(*mBuffer),
            [self = shared_from_this()](
                const std::error_code ec, std::size_t bytes_transferred) {
                if (!ec)
                {
                    self->mBuffer->clear();

                    if (!self->mServiceProvider->GetService<MpMcBufferPool>()
                             ->try_push(self->mBuffer))
                        delete self->mBuffer;

                    if (self)
                        self->mLogger->LogInformation("Message sent: {} bytes",
                                                      bytes_transferred);
                }
                else
                {
                    if (self)
                        self->mLogger->LogError(
                            "Error writing: {}", ec.message());
                }
            });
    }

    net::ip::tcp::socket mSocket;

    std::vector<char>* mBuffer;

    Ref<skr::Logger<HttpConnection>> mLogger;
    Ref<skr::ServiceProvider>        mServiceProvider;
    Ref<HttpRequestParser>           mHttpRequestParser;
    Ref<MiddlewareProvider>          mMiddlewareProvider;
    Ref<Router>                      mRouter;
};
