/**
 * @file Middleware/SecurityHeaders.hpp
 * @brief Middleware that emits defensive HTTP response headers
 *        (CSP-friendly defaults, HSTS, etc.).
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE
{
    namespace
    {
        /**
         * @brief Strip CR/LF bytes from a header value before it is written
         *        to the response.
         *
         * RFC 7230 forbids bare CR or LF in the field-value of an HTTP
         * header. A user-supplied value that contains them is therefore
         * unusable as a header value; rather than emit a header that
         * response-splits the connection, collapse each offending byte
         * to a single ASCII space and trim leading/trailing whitespace
         * produced by collapsing.
         *
         * @param value  Header value as configured by the caller.
         * @return       Sanitised value safe for serialisation.
         */
        std::string sanitizeHeaderValue(std::string_view value)
        {
            std::string out;
            out.reserve(value.size());
            bool prevSpace = false;
            for (char c : value)
            {
                if (c == '\r' || c == '\n' || c == ' ')
                {
                    if (!prevSpace)
                    {
                        out.push_back(' ');
                        prevSpace = true;
                    }
                    continue;
                }
                out.push_back(c);
                prevSpace = false;
            }
            if (!out.empty() && out.front() == ' ')
                out.erase(out.begin());
            if (!out.empty() && out.back() == ' ')
                out.pop_back();
            return out;
        }
    } // namespace

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

        /// @c Referrer-Policy value (e.g. @c
        /// "strict-origin-when-cross-origin").
        std::optional<std::string> referrerPolicy =
            "strict-origin-when-cross-origin";

        /// @c Strict-Transport-Security value (only emitted when set).
        std::optional<std::string> strictTransportSecurity = std::nullopt;

        /// @c Permissions-Policy value (e.g. @c "geolocation=(),
        /// microphone=()").
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
            auto        writeHeader =
                [&response](const char*                       name,
                            const std::optional<std::string>& v) {
                    if (!v)
                        return;
                    auto sanitised = sanitizeHeaderValue(*v);
                    if (sanitised.empty())
                        return;
                    response.headers[name] = std::move(sanitised);
                };
            writeHeader("X-Content-Type-Options", o.contentTypeOptions);
            writeHeader("X-Frame-Options", o.frameOptions);
            writeHeader("Referrer-Policy", o.referrerPolicy);
            writeHeader("Strict-Transport-Security", o.strictTransportSecurity);
            writeHeader("Permissions-Policy", o.permissionsPolicy);
            writeHeader("Cross-Origin-Opener-Policy",
                        o.crossOriginOpenerPolicy);
            writeHeader("Cross-Origin-Resource-Policy",
                        o.crossOriginResourcePolicy);
            writeHeader("Cross-Origin-Embedder-Policy",
                        o.crossOriginEmbedderPolicy);

            next();
        }

      private:
        SecurityHeadersOptions mOptions;
    };

} // namespace BALDR_NAMESPACE
