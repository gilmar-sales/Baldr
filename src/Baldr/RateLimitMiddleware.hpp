#pragma once

#include "IMiddleware.hpp"
#include "RateLimiter.hpp"

class RateLimitMiddleware : public IMiddleware
{
  public:
    explicit RateLimitMiddleware(
        const skr::Arc<RateLimiter>&                      rateLimiter,
        const skr::Arc<skr::Logger<RateLimitMiddleware>>& logger) :
        mRateLimiter(rateLimiter), mLogger(logger)
    {
    }

    void Handle(const HttpRequest& request, HttpResponse& response,
                const NextMiddleware& next) override
    {
        if (!mRateLimiter->isAllowed(request.clientIp))
        {
            response.statusCode = StatusCode::TooManyRequests;
            response.body =
                R"({ "status": 429, "message": "Too Many Requests" })";
            response.headers["Content-Type"] = "application/json";
            response.headers["Content-Length"] =
                std::to_string(response.body.size());

            mLogger->LogWarning("Endpoint {} - Has been limited", request.path);
            return;
        }

        next();
    }

  private:
    skr::Arc<RateLimiter>                      mRateLimiter;
    skr::Arc<skr::Logger<RateLimitMiddleware>> mLogger;
};