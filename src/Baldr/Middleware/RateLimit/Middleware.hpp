/**
 * @file Middleware/RateLimit/Middleware.hpp
 * @brief Middleware that wraps a @ref RateLimiter and short-circuits with
 *        429 when the per-client budget is exhausted.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string_view>

#include <Baldr/Middleware/IMiddleware.hpp>
#include <Baldr/Middleware/RateLimit/Limiter.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Middleware that enforces a per-client rate limit using a
     *        @ref RateLimiter.
     *
     * By default the limiter is keyed on @c HttpRequest::clientIp (the
     * TCP peer). When @ref useForwardedFor is @c true the middleware
     * walks the @c X-Forwarded-For chain from the right, skipping hops
     * listed in @ref trustedProxies, and uses the first untrusted IP.
     * This prevents trivially spoofing the rate-limit key by sending a
     * custom @c X-Forwarded-For header from a non-trusted peer.
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

        /**
         * @brief Honour @c X-Forwarded-For when picking the rate-limit key.
         *
         * @param value @c true to consult @c X-Forwarded-For.
         */
        void setUseForwardedFor(bool value) { mUseForwardedFor = value; }

        /**
         * @brief Treat IPs matching one of these prefixes as trusted hops
         *        when picking the leftmost untrusted @c X-Forwarded-For
         *        entry. Substring match against the textual IP, e.g.
         *        @c "10.", @c "192.168.". Empty by default.
         */
        void setTrustedProxies(std::vector<std::string> prefixes)
        {
            mTrustedProxies = std::move(prefixes);
        }

        void Handle(HttpRequest& request, HttpResponse& response,
                    const NextMiddleware& next) override
        {
            const std::string key = pickKey(request);
            if (!mRateLimiter->isAllowed(key))
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
        static bool startsWithAny(std::string_view                s,
                                  const std::vector<std::string>& prefixes)
        {
            for (const auto& p : prefixes)
            {
                if (!p.empty() && s.substr(0, p.size()) == p)
                    return true;
            }
            return false;
        }

        std::string pickKey(const HttpRequest& request) const
        {
            if (!mUseForwardedFor)
                return request.clientIp;

            auto xffIt = request.headers.find("x-forwarded-for");
            if (xffIt == request.headers.end() || xffIt->second.empty())
                return request.clientIp;

            const auto& xff   = xffIt->second;
            std::size_t start = 0;
            while (start < xff.size())
            {
                auto comma = xff.find(',', start);
                if (comma == std::string::npos)
                    comma = xff.size();
                std::size_t a = start;
                while (a < comma && (xff[a] == ' ' || xff[a] == '\t'))
                    ++a;
                std::size_t b = comma;
                while (b > a && (xff[b - 1] == ' ' || xff[b - 1] == '\t'))
                    --b;
                std::string hop(xff.data() + a, b - a);
                if (!hop.empty() && !startsWithAny(hop, mTrustedProxies))
                    return hop;
                if (comma == xff.size())
                    break;
                start = comma + 1;
            }
            return request.clientIp;
        }

        skr::Arc<RateLimiter>                      mRateLimiter;
        skr::Arc<skr::Logger<RateLimitMiddleware>> mLogger;
        bool                                       mUseForwardedFor { false };
        std::vector<std::string>                   mTrustedProxies;
    };

} // namespace BALDR_NAMESPACE