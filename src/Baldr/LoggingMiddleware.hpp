#pragma once

#include "IMiddleware.hpp"

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
        mLogger->LogInformation("Request received: '{}' '{}' for path: '{}'",
                                request.version, request.method, request.path);
        auto begin = std::chrono::system_clock::now();
        next();
        auto end = std::chrono::system_clock::now();

        mLogger->LogInformation(
            "Request finished in {}",
            std::chrono::duration_cast<std::chrono::microseconds>(end - begin));
    }

  private:
    Ref<skr::Logger<LoggingMiddleware>> mLogger;
};