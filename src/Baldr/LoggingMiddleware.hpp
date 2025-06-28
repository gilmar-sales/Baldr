#pragma once

#include "IMiddleware.hpp"

#include "rfl/enums.hpp"
#include <chrono>

class LoggingMiddleware final : public IMiddleware
{
  public:
    LoggingMiddleware(Ref<skr::Logger<LoggingMiddleware>> logger) :
        mLogger(logger)
    {
    }

    ~LoggingMiddleware() = default;

    void Handle(const HttpRequest& request, HttpResponse& response,
                const NextMiddleware& next) override
    {
        const auto method = rfl::enum_to_string(request.method);

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
    Ref<skr::Logger<LoggingMiddleware>> mLogger;
};
