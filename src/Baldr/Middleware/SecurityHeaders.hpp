/**
 * @file Middleware/SecurityHeaders.hpp
 * @brief Middleware that emits defensive HTTP response headers
 *        (CSP-friendly defaults, HSTS, etc.).
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <optional>
#include <string>

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Configuration for @ref SecurityHeadersMiddleware.
     *
     * Each field corresponds to one outgoing header. Setting a field to
     * @c std::nullopt disables that header; setting it to the empty
     * string sends the header with an empty value.
     */
    struct SecurityHeadersOptions
    {
        /// @c X-Content-Type-Options value (e.g. @c "nosniff").
        std::optional<std::string> contentTypeOptions = "nosniff";

        /// @c X-Frame-Options value (e.g. @c "DENY" or @c "SAMEORIGIN").
        std::optional<std::string> frameOptions = "DENY";

        /// @c Referrer-Policy value (e.g. @c "strict-origin-when-cross-origin").
        std::optional<std::string> referrerPolicy =
            "strict-origin-when-cross-origin";

        /// @c Strict-Transport-Security value (only emitted when set).
        std::optional<std::string> strictTransportSecurity = std::nullopt;

        /// @c Permissions-Policy value (e.g. @c "geolocation=(), microphone=()").
        std::optional<std::string> permissionsPolicy = std::nullopt;

        /// @c Cross-Origin-Opener-Policy value (e.g. @c "same-origin").
        std::optional<std::string> crossOriginOpenerPolicy = "same-origin";

        /// @c Cross-Origin-Resource-Policy value.
        std::optional<std::string> crossOriginResourcePolicy = "same-origin";

        /// @c Cross-Origin-Embedder-Policy value (only emitted when set).
        std::optional<std::string> crossOriginEmbedderPolicy = std::nullopt;
    };

    /**
     * @brief Middleware that writes the configured defensive headers on
     *        every response.
     */
    class SecurityHeadersMiddleware final : public IMiddleware
    {
      public:
        /**
         * @brief Construct with the given options.
         */
        explicit SecurityHeadersMiddleware(
            SecurityHeadersOptions options = {}) : mOptions(std::move(options))
        {
        }

        ~SecurityHeadersMiddleware() override = default;

        void Handle(HttpRequest&          request,
                    HttpResponse&         response,
                    const NextMiddleware& next) override
        {
            (void) request;

            const auto& o = mOptions;
            if (o.contentTypeOptions)
                response.headers["X-Content-Type-Options"] =
                    *o.contentTypeOptions;
            if (o.frameOptions)
                response.headers["X-Frame-Options"] = *o.frameOptions;
            if (o.referrerPolicy)
                response.headers["Referrer-Policy"] = *o.referrerPolicy;
            if (o.strictTransportSecurity)
                response.headers["Strict-Transport-Security"] =
                    *o.strictTransportSecurity;
            if (o.permissionsPolicy)
                response.headers["Permissions-Policy"] = *o.permissionsPolicy;
            if (o.crossOriginOpenerPolicy)
                response.headers["Cross-Origin-Opener-Policy"] =
                    *o.crossOriginOpenerPolicy;
            if (o.crossOriginResourcePolicy)
                response.headers["Cross-Origin-Resource-Policy"] =
                    *o.crossOriginResourcePolicy;
            if (o.crossOriginEmbedderPolicy)
                response.headers["Cross-Origin-Embedder-Policy"] =
                    *o.crossOriginEmbedderPolicy;

            next();
        }

      private:
        SecurityHeadersOptions mOptions;
    };

} // namespace BALDR_NAMESPACE
