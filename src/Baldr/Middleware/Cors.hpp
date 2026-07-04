/**
 * @file Middleware/Cors.hpp
 * @brief CORS middleware that emits the relevant response headers and
 *        short-circuits @c OPTIONS pre-flight requests.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>
#include <unordered_set>

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Configuration for @ref CorsMiddleware.
     */
    struct CorsOptions
    {
        /// Value sent in @c Access-Control-Allow-Origin.
        std::string allowOrigin = "*";
        /// Methods advertised in @c Access-Control-Allow-Methods.
        std::unordered_set<std::string> allowMethods = {
            "GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS"
        };
        /// Headers advertised in @c Access-Control-Allow-Headers.
        std::unordered_set<std::string> allowHeaders = { "Content-Type",
                                                         "Authorization" };
        /// When @c true, send @c Access-Control-Allow-Credentials: true.
        bool allowCredentials = false;
        /// Cache lifetime advertised via @c Access-Control-Max-Age (seconds).
        int maxAge = 86400;
    };

    /**
     * @brief Middleware that adds CORS response headers and handles
     *        @c OPTIONS pre-flight requests with a 204 status.
     *
     * Pre-flight responses short-circuit the chain; other requests continue
     * to the next middleware / route handler.
     */
    class CorsMiddleware final : public IMiddleware
    {
      public:
        /**
         * @brief Construct the middleware with the given options.
         */
        explicit CorsMiddleware(CorsOptions options = {}) :
            mOptions(std::move(options))
        {
        }

        ~CorsMiddleware() override = default;

        void Handle(HttpRequest&          request,
                    HttpResponse&         response,
                    const NextMiddleware& next) override
        {
            response.headers["Access-Control-Allow-Origin"] =
                mOptions.allowOrigin;
            response.headers["Access-Control-Allow-Methods"] =
                join(mOptions.allowMethods, ", ");
            response.headers["Access-Control-Allow-Headers"] =
                join(mOptions.allowHeaders, ", ");
            response.headers["Access-Control-Max-Age"] =
                std::to_string(mOptions.maxAge);
            if (mOptions.allowCredentials)
                response.headers["Access-Control-Allow-Credentials"] = "true";

            if (request.method == HttpMethod::Options)
            {
                response.statusCode = StatusCode::NoContent;
                return;
            }

            next();
        }

      private:
        static std::string join(const std::unordered_set<std::string>& items,
                                const std::string&                     sep)
        {
            std::string out;
            for (const auto& item : items)
            {
                if (!out.empty())
                    out += sep;
                out += item;
            }
            return out;
        }

        CorsOptions mOptions;
    };

} // namespace BALDR_NAMESPACE