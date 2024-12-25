#pragma once

#include <iostream>
#include <memory>
#include <asio/read_until.hpp>
#include <asio/ip/tcp.hpp>

#include "HttpResponse.hpp"


inline std::string trim(const std::string &str) {
    const size_t start = str.find_first_not_of(" \t\n\r\f\v"); // Find first non-whitespace
    const size_t end = str.find_last_not_of(" \t\n\r\f\v"); // Find last non-whitespace

    // If the string is empty or contains only whitespace, return an empty string
    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }

    return str.substr(start, end - start + 1); // Extract substring with leading and trailing whitespaces removed
}

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    explicit HttpSession(asio::ip::tcp::socket socket,
                         const std::shared_ptr<ServiceProvider> &serviceProvider,
                         const std::shared_ptr<MiddlewareFactoryList> &middlewareFactories)
        : mSocket(std::move(socket)),
          mServiceProvider(serviceProvider),
          mMiddlewareFactories(middlewareFactories) {
    }

    void start() {
        readRequest();
    }

private:
    void readRequest() {
        auto self = shared_from_this();
        asio::async_read_until(
            mSocket, asio::dynamic_buffer(mData), "\r\n\r\n",
            [this, self](const std::error_code ec, const std::size_t bytes_transferred) {
                if (!ec) {
                    processRequest(bytes_transferred);
                } else {
                    std::cerr << "Error reading request: " << ec.message() << std::endl;
                }
            });
    }

    void processRequest(std::size_t bytes_transferred) {
        // Extract the request
        std::string request(mData.substr(0, bytes_transferred));
        mData.erase(0, bytes_transferred);

        auto httpRequest = HttpRequest();

        std::istringstream request_stream(request);
        request_stream >> httpRequest.method >> httpRequest.path >> httpRequest.version;

        std::string line;

        while (std::getline(request_stream, line) && request_stream.good()) {
            if (auto colon = line.find(':'); colon != std::string::npos) {
                auto key = trim(line.substr( 0, colon));
                auto value = trim(line.substr(colon + 1));
                httpRequest.headers[key] = value;
            }
        }

        if (httpRequest.headers.contains("Content-Length")) {
            httpRequest.body = trim(mData);
        }

        // Simple routing logic
        auto httpResponse = HttpResponse(httpRequest);

        for (const auto &middlewareFactory: *mMiddlewareFactories) {
            middlewareFactory(mServiceProvider)->Handle(httpRequest, httpResponse);
        }

        if (httpRequest.path == "/") {
            httpResponse.body = "<html><h1>Welcome to the Asio HTTP Server</h1></html>";
        } else if (httpRequest.path == "/hello") {
            httpResponse.body = "<html><h1>Hello, World!</h1></html>";
        } else {
            httpResponse.body = "<html><h1>404 Not Found</h1></html>";
        }

        httpResponse.headers["Content-Type"] = "text/html";
        httpResponse.headers["Content-Length"] = std::to_string(httpResponse.body.size());

        httpResponse.cookies["XSRF-TOKEN"] = {
            .value = "Lorem ipsum",
            .sameSite = SameSite::Lax,
            .httpOnly = false,
            .secure = true,
        };

        // Create the HTTP response
        std::ostringstream response_stream;
        response_stream << httpResponse.version << " " << httpResponse.statusCode << " OK\r\n";

        for (const auto &[headerName,headerValue]: httpResponse.headers) {
            response_stream << headerName << ": " << headerValue << "\r\n";
        }

        for (const auto &[cookieName, cookieOptions]: httpResponse.cookies) {
            response_stream << "Set-Cookie: " << cookieName << "=" << cookieOptions.value;

            if (cookieOptions.sameSite.has_value())
                switch (cookieOptions.sameSite.value()) {
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

        response_stream
                << "Connection: close\r\n\r\n"
                << httpResponse.body;

        mResponse = response_stream.str();
        writeResponse();
    }

    void writeResponse() {
        auto self = shared_from_this();
        asio::async_write(
            mSocket, asio::buffer(mResponse),
            [this, self](const std::error_code ec, std::size_t /*bytes_transferred*/) {
                if (!ec) {
                    mSocket.shutdown(asio::ip::tcp::socket::shutdown_send);
                } else {
                    std::cerr << "Error sending response: " << ec.message() << std::endl;
                }
            });
    }

    asio::ip::tcp::socket mSocket;
    std::string mData;
    std::string mResponse;
    std::shared_ptr<ServiceProvider> mServiceProvider;
    std::shared_ptr<MiddlewareFactoryList> mMiddlewareFactories;
};
