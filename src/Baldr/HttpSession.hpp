#pragma once

#define ASIOP_STANDALONE
#include <asio.hpp>

#include <iostream>
#include <memory>

#include "HttpResponse.hpp"
#include "MiddlewareProvider.hpp"
#include "PathMatcher.hpp"

#include <ranges>

inline std::string trim(const std::string& str)
{
    const size_t start = str.find_first_not_of(" \t\n\r\f\v");

    const size_t end = str.find_last_not_of(" \t\n\r\f\v");

    if (start == std::string::npos || end == std::string::npos)
    {
        return "";
    }

    return str.substr(start, end - start + 1);
}

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
  public:
    explicit HttpSession(const Ref<skr::ServiceProvider>& serviceProvider,
                         const Ref<MiddlewareProvider>&   middlewareProvider,
                         const Ref<PathMatcher>&          pathMatcher,
                         asio::ip::tcp::socket            socket) :
        mServiceProvider(serviceProvider),
        mMiddlewareProvider(middlewareProvider), mPathMatcher(pathMatcher),
        mSocket(std::move(socket))
    {
        mLogger = mServiceProvider->GetService<skr::Logger<HttpSession>>();
    }

    void start() { readRequest(); }

  private:
    void readRequest()
    {
        auto self = shared_from_this();
        async_read_until(mSocket, asio::dynamic_buffer(mData), "\r\n\r\n",
                         [self](const std::error_code ec,
                                const std::size_t     bytes_transferred) {
                             if (!ec)
                             {
                                 self->processRequest(bytes_transferred);
                             }
                             else
                             {
                                 self->mLogger->LogError(
                                     "Error reading request: {}", ec.message());
                             }
                         });
    }

    void processRequest(std::size_t bytes_transferred)
    {
        std::string request(mData.substr(0, bytes_transferred));

        auto httpRequest = HttpRequest {};

        httpRequest.clientIp = mSocket.remote_endpoint().address().to_string();

        std::istringstream request_stream(request);
        request_stream >> httpRequest.method >> httpRequest.path >>
            httpRequest.version;

        auto queryIndex = httpRequest.path.find('?');

        if (queryIndex != std::string::npos)
        {
            auto params =
                std::string_view(httpRequest.path.begin() + queryIndex,
                                 httpRequest.path.end());

            for (const auto& part : params | std::views::split('&'))
            {
                std::string pair(part.begin(), part.end());
                auto        eq_pos = pair.find('=');
                if (eq_pos != std::string::npos)
                {
                    httpRequest.params[pair.substr(0, eq_pos)] =
                        pair.substr(eq_pos + 1);
                }
                else
                {
                    httpRequest.params[pair] = "";
                }
            }

            httpRequest.path.resize(queryIndex);
        }

        std::string line;

        while (std::getline(request_stream, line) && request_stream.good())
        {
            if (auto colon = line.find(':'); colon != std::string::npos)
            {
                auto key                 = trim(line.substr(0, colon));
                auto value               = trim(line.substr(colon + 1));
                httpRequest.headers[key] = value;
            }
        }

        if (httpRequest.headers.contains("Content-Length"))
        {
            httpRequest.body = trim(request);
        }

        // Simple routing logic
        auto httpResponse = HttpResponse(httpRequest);

        auto current = mMiddlewareProvider->begin();

        NextMiddleware nextLambda = [&]() -> void {
            auto nextIt = current + 1;

            if (nextIt != mMiddlewareProvider->end())
            {
                current += 1;

                return (*nextIt)(mServiceProvider)
                    ->Handle(httpRequest, httpResponse, nextLambda);
            }

            const auto& handler =
                mPathMatcher->match(httpRequest.method, httpRequest.path);

            if (handler.has_value())
            {
                handler.value()(httpRequest, httpResponse, mServiceProvider);
            }
            else
            {
                httpResponse.statusCode = StatusCode::NotFound;
            }

            if (!httpResponse.body.empty())
                httpResponse.headers["Content-Length"] =
                    std::to_string(httpResponse.body.size());
        };

        (*current)(mServiceProvider)
            ->Handle(httpRequest, httpResponse, nextLambda);

        // Create the HTTP response
        std::ostringstream response_stream;
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

        mResponse = response_stream.str();
        writeResponse();
    }

    void writeResponse()
    {
        auto self = shared_from_this();

        async_write(
            mSocket, asio::buffer(mResponse),
            [self](const std::error_code ec, std::size_t bytes_transferred) {
                if (!ec)
                {
                    self->mLogger->LogInformation("Message sent: {} bytes",
                                                  bytes_transferred);
                }
                else
                {
                    self->mLogger->LogError("Error writing: {}", ec.message());
                }
            });
    }

    asio::ip::tcp::socket mSocket;
    std::string           mData;
    std::string           mResponse;

    Ref<skr::Logger<HttpSession>> mLogger;
    Ref<skr::ServiceProvider>     mServiceProvider;
    Ref<MiddlewareProvider>       mMiddlewareProvider;
    Ref<PathMatcher>              mPathMatcher;
};
