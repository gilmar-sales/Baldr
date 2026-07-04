/**
 * @file Middleware/RateLimit/Middleware.hpp
 * @brief Middleware that wraps a @ref RateLimiter and short-circuits with
 *        429 when the per-client budget is exhausted.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Baldr/Middleware/IMiddleware.hpp>
#include <Baldr/Middleware/RateLimit/Limiter.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Middleware that enforces a per-client rate limit using a
     *        @ref RateLimiter keyed by @c HttpRequest::clientIp.
     */
    class RateLimitMiddleware : public IMiddleware
    {
      public:
        /**
         * @brief Construct with the limiter and logger to use.
         */
        explicit RateLimitMiddleware(
            const skr::Arc<RateLimiter>&                      rateLimiter,
            const skr::Arc<skr::Logger<RateLimitMiddleware>>& logger) :
            mRateLimiter(rateLimiter), mLogger(logger)
        {
        }

        void Handle(HttpRequest& request, HttpResponse& response,
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

                mLogger->LogWarning("Endpoint {} - Has been limited",
                                    request.path);
                return;
            }

            next();
        }

      private:
        skr::Arc<RateLimiter>                      mRateLimiter;
        skr::Arc<skr::Logger<RateLimitMiddleware>> mLogger;
    };

} // namespace BALDR_NAMESPACE