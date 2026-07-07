/**
 * @file Middleware/Compression/Middleware.hpp
 * @brief Middleware that gzip-compresses buffered responses whose
 *        Content-Type qualifies and whose size exceeds the threshold.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>
#include <unordered_set>

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Configuration for @ref CompressionMiddleware.
     */
    struct CompressionOptions
    {
        /**
         * @brief Only compress responses whose Content-Type starts with one
         *        of these prefixes (case-insensitive). Defaults to text-like
         *        types where compression pays off.
         */
        std::unordered_set<std::string> mimeTypePrefixes = {
            "text/",
            "application/json",
            "application/javascript",
            "application/xml",
            "application/xhtml+xml",
            "image/svg+xml",
        };

        /**
         * @brief Skip responses smaller than this many bytes. Defaults to
         *        1024 because compression overhead rarely pays off on tiny
         *        bodies.
         */
        std::size_t minBodyBytes = 1024;

        /**
         * @brief Compression level passed to zlib (1..9). @c -1 selects
         *        zlib's default (typically 6).
         */
        int level = -1;
    };

    /**
     * @brief Middleware that gzip-compresses qualifying buffered responses
     *        and updates the @c Content-Encoding / @c Content-Length
     *        headers accordingly.
     */
    class CompressionMiddleware final : public IMiddleware
    {
      public:
        /**
         * @brief Construct the middleware with the given options.
         */
        explicit CompressionMiddleware(CompressionOptions options = {}) :
            mOptions(std::move(options))
        {
        }

        ~CompressionMiddleware() override = default;

        void Handle(HttpRequest&          request,
                    HttpResponse&         response,
                    const NextMiddleware& next) override;

      private:
        /**
         * @brief ASCII-only @c std::tolower replacement (the locale-independent
         *        variant required for HTTP header parsing).
         */
        static std::string toLowerAscii(std::string_view s);

        /**
         * @brief @c true when @p ctype's @c "/" subtype matches one of the
         *        @c prefixes (e.g. @c "text/", @c "application/json").
         */
        static bool mimeAllowed(
            const std::string&                     ctype,
            const std::unordered_set<std::string>& prefixes);

      public:
        // Public helper for unit tests; not part of the framework's
        // user-facing API.
        static bool mimeAllowedForTest(
            const std::string&                     ctype,
            const std::unordered_set<std::string>& prefixes)
        {
            return mimeAllowed(ctype, prefixes);
        }

        CompressionOptions mOptions;
    };

} // namespace BALDR_NAMESPACE
