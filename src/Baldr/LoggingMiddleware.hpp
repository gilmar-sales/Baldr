#pragma once

#include "IMiddleware.hpp"

#include <chrono>
#include <meta>

#include <Skirnir/Common/Reflection.hpp>

class LoggingMiddleware final : public IMiddleware
{
  public:
    LoggingMiddleware(skr::Arc<skr::Logger<LoggingMiddleware>> logger) :
        mLogger(logger)
    {
    }

    ~LoggingMiddleware() = default;

    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override
    {
        const auto method = refl::enum_to_string(request.method);

        mLogger->LogInformation(
            "Request - '{}' '{}' '{}'", request.version, method, request.path);

        auto begin = std::chrono::system_clock::now();

        next();

        auto end = std::chrono::system_clock::now();

        mLogger->LogInformation(
            "Response - '{}' '{}' '{}' - {} - {}",
            static_cast<int>(response.statusCode), method, request.path,
            std::chrono::duration_cast<std::chrono::microseconds>(end - begin),
            request.clientIp);
    }

  private:
    skr::Arc<skr::Logger<LoggingMiddleware>> mLogger;
};
