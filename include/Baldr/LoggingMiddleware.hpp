#pragma once
#include "IMiddleware.hpp"

class LoggingMiddleware final : public IMiddleware
{
  public:
    LoggingMiddleware(Ref<skr::Logger<LoggingMiddleware>> logger) :
        mLogger(logger)
    {
    }

    ~LoggingMiddleware() override
    {
        mLogger->LogInformation("Logging finished");
    }

    void Handle(const HttpRequest& request, HttpResponse& response,
                NextMiddleware& next) override
    {
        mLogger->LogInformation("Request received: '{}' '{}' for path: '{}'",
                                request.version, request.method, request.path);

        next();
    }

  private:
    Ref<skr::Logger<LoggingMiddleware>> mLogger;
};