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
    }

    void start() { run(); }

  private:
    skr::Task<> run()
    {
        bool keepAlive = true;

        while (keepAlive)
        {
            mBuffer = mServiceProvider->GetService<MpMcBufferPool>()->try_pop();

            std::error_code ec;
            std::size_t     headerByteCount = 0;

            co_await readHeaders(headerByteCount, ec);
            if (ec)
            {
                if (ec != net::error::eof &&
                    ec != net::error::connection_reset &&
                    ec != net::error::operation_aborted)
                {
                    mLogger->LogError("read error: {}", ec.message());
                }
                releaseBuffer();
                break;
            }

            co_await ensureBody(headerByteCount, ec);
            if (ec)
            {
                mLogger->LogError("read body error: {}", ec.message());
                releaseBuffer();
                break;
            }

            std::string request(mBuffer->data(), mBuffer->size());
            bool       clientWantsClose = false;
            keepAlive                  = decideKeepAlive(request,
                                              headerByteCount,
                                              clientWantsClose);

            bool closeAfterWrite = false;
            co_await processRequest(request,
                                    headerByteCount,
                                    clientWantsClose,
                                    closeAfterWrite);

            co_await writeResponse(ec);
            releaseBuffer();

            if (ec)
            {
                mLogger->LogError("write error: {}", ec.message());
                break;
            }

            if (closeAfterWrite)
                keepAlive = false;
        }

        std::error_code ignored;
        mSocket.shutdown(net::socket_base::shutdown_send, ignored);
        co_return;
    }

    skr::Task<> readHeaders(std::size_t& headerByteCount, std::error_code& ec)
    {
        mBuffer->clear();
        co_await baldr::AsioAwait<std::size_t>(
            mSocket.get_executor(),
            net::async_read_until(mSocket, net::dynamic_buffer(*mBuffer),
                                  "\r\n\r\n", net::use_awaitable));

        auto end = std::string_view(mBuffer->data(), mBuffer->size())
                       .find("\r\n\r\n");
        headerByteCount = (end == std::string_view::npos)
                              ? mBuffer->size()
                              : end + 4;
        ec = {};
        co_return;
    }

    skr::Task<> ensureBody(std::size_t headerByteCount, std::error_code& ec)
    {
        ec = {};
        std::size_t contentLength = 0;
        bool        hasBody       = false;

        std::string_view headerView(mBuffer->data(), headerByteCount);
        for (auto pos = headerView.find("Content-Length:");
                 pos != std::string_view::npos;
                 pos = headerView.find("Content-Length:", pos + 1))
        {
            auto lineEnd = headerView.find("\r\n", pos);
            if (lineEnd == std::string_view::npos)
                lineEnd = headerView.size();
            std::string_view line(headerView.data() + pos + 15,
                                  lineEnd - (pos + 15));
            auto begin = line.find_first_not_of(" \t");
            if (begin != std::string_view::npos)
            {
                line.remove_prefix(begin);
                try
                {
                    contentLength = std::stoul(std::string(line));
                    hasBody       = true;
                }
                catch (...)
                {
                }
            }
            break;
        }

        if (!hasBody)
            co_return;

        if (mBuffer->size() >= headerByteCount + contentLength)
            co_return;

        co_await baldr::AsioAwait<std::size_t>(
            mSocket.get_executor(),
            net::async_read(
                mSocket,
                net::dynamic_buffer(*mBuffer),
                net::transfer_exactly(contentLength -
                                      (mBuffer->size() - headerByteCount)),
                net::use_awaitable));
        co_return;
    }

    static bool decideKeepAlive(const std::string& request,
                                std::size_t        headerByteCount,
                                bool&              clientWantsClose)
    {
        std::string_view headerView(request.data(), headerByteCount);
        bool             http10 =
            headerView.find("HTTP/1.0") != std::string_view::npos;

        // Default: HTTP/1.1 = keep-alive, HTTP/1.0 = close.
        bool keepAlive = !http10;

        // Inspect the Connection header. The parser lowercases header names,
        // but we are reading the raw request here, so we case-fold.
        auto findHeader = [&](std::string_view needle) {
            auto pos = headerView.find(needle);
            while (pos != std::string_view::npos)
            {
                if (pos == 0 || headerView[pos - 1] == '\n')
                    return pos;
                pos = headerView.find(needle, pos + 1);
            }
            return pos;
        };

        auto connPos = findHeader("Connection:");
        if (connPos != std::string_view::npos)
        {
            auto lineEnd = headerView.find("\r\n", connPos);
            if (lineEnd == std::string_view::npos)
                lineEnd = headerView.size();
            std::string_view value(headerView.data() + connPos + 11,
                                   lineEnd - (connPos + 11));
            std::string lowered;
            lowered.reserve(value.size());
            for (char c : value)
            {
                if (c == ' ' || c == '\t')
                    continue;
                lowered.push_back(
                    static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            if (lowered.find("close") != std::string::npos)
                keepAlive = false;
            else if (lowered.find("keep-alive") != std::string::npos)
                keepAlive = true;
        }

        clientWantsClose = !keepAlive;
        return keepAlive;
    }

    skr::Task<> processRequest(const std::string& request,
                               std::size_t        headerByteCount,
                               bool               clientWantsClose,
                               bool&              closeAfterWrite)
    {
        auto httpRequestParse =
            mHttpRequestParser->parse(request, headerByteCount);

        if (!httpRequestParse.success)
        {
            mLogger->LogError("Failed to parse HTTP request: {}",
                              httpRequestParse.error);
            mBuffer->assign_range("HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Length: 0\r\n"
                                  "Connection: close\r\n"
                                  "\r\n");
            closeAfterWrite = true;
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
            mBuffer->assign_range("HTTP/1.1 404 Not Found\r\n"
                                  "Content-Length: 0\r\n"
                                  "Connection: close\r\n"
                                  "\r\n");
            closeAfterWrite = true;
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

        // Detect whether the handler/middleware signalled Connection: close.
        bool serverWantsClose = false;
        auto closeIt = httpResponse.headers.find("Connection");
        if (closeIt != httpResponse.headers.end())
        {
            std::string lowered(closeIt->second);
            std::transform(lowered.begin(),
                           lowered.end(),
                           lowered.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lowered == "close")
                serverWantsClose = true;
        }
        if (serverWantsClose)
            closeAfterWrite = true;

        std::ostringstream response_stream {};
        response_stream << httpResponse.version << " "
                        << httpResponse.statusCode << " OK\r\n";

        for (const auto& [headerName, headerValue] : httpResponse.headers)
        {
            if (serverWantsClose && headerName == "Connection")
                continue;
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

        if (serverWantsClose || clientWantsClose)
            response_stream << "Connection: close\r\n";

        response_stream << "\r\n" << httpResponse.body;

        mBuffer->clear();
        mBuffer->assign_range(response_stream.str());
        co_return;
    }

    skr::Task<> writeResponse(std::error_code& ec)
    {
        co_await baldr::AsioAwait<std::size_t>(
            mSocket.get_executor(),
            net::async_write(mSocket, net::buffer(*mBuffer),
                             net::use_awaitable));
        ec = {};
        co_return;
    }

    void releaseBuffer()
    {
        if (mBuffer == nullptr)
            return;
        if (!mServiceProvider->GetService<MpMcBufferPool>()->try_push(mBuffer))
            delete mBuffer;
        mBuffer = nullptr;
    }

    net::ip::tcp::socket mSocket;

    std::vector<char>* mBuffer = nullptr;

    skr::Arc<skr::Logger<HttpConnection>> mLogger;
    skr::Arc<skr::ServiceProvider>        mServiceProvider;
    skr::Arc<HttpRequestParser>           mHttpRequestParser;
    skr::Arc<MiddlewareProvider>          mMiddlewareProvider;
    skr::Arc<Router>                      mRouter;
};
