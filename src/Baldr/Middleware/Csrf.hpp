/**
 * @file Middleware/Csrf.hpp
 * @brief Double-submit-cookie CSRF protection middleware.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <string_view>

#include <Baldr/Hosting/SecureRandom.hpp>
#include <Baldr/Hosting/StringHelpers.hpp>
#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Configuration for @ref CsrfMiddleware.
     *
     * Implements the double-submit pattern: a cookie carries the token and
     * the client must echo it in a request header.
     *
     * @note Threat model: the double-submit-cookie pattern is broken if
     *       a subdomain can set cookies on the target origin
     *       (@c example.com subdomains may set cookies read by
     *       @c app.example.com). For defence, restrict the CSRF cookie
     *       @c domain and prefer a synchronizer-token pattern with a
     *       server-side session store if your deployment has subdomains
     *       under partial attacker control.
     */
    struct CsrfOptions
    {
        /// CSRF cookie name. Default @c "XSRF-TOKEN" (matches Angular).
        std::string cookieName = "XSRF-TOKEN";

        /// Request header the client must set to the cookie value.
        std::string headerName = "X-XSRF-TOKEN";

        /// Methods considered "unsafe" and therefore protected. Safe methods
        /// (GET/HEAD/OPTIONS) skip the check.
        std::set<HttpMethod> protectedMethods = {
            HttpMethod::Post, HttpMethod::Put, HttpMethod::Patch,
            HttpMethod::Delete
        };

        /// Path prefixes excluded from CSRF protection (e.g. @c
        /// "/api/webhooks/").
        std::set<std::string> exemptPathPrefixes {};

        /**
         * @brief When @c true, the middleware issues a CSRF cookie on safe
         *        requests that lack one. When @c false, callers must set the
         *        cookie themselves.
         */
        bool issueCookieOnSafeRequest = true;

        /// Cookie attributes used when issuing the token.
        bool cookieHttpOnly =
            false; ///< Must be readable from JS, so default is @c false.
        bool cookieSecure = false; ///< Set to @c true when serving over HTTPS.
        long cookieMaxAge = 0; ///< @c Max-Age in seconds. 0 = session cookie.
        /// @c SameSite attribute for the issued cookie. Defaults to
        /// @c Strict (no cross-site requests carry the token), which is
        /// the strongest CSRF posture compatible with typical apps. Set
        /// to @c Lax if the cookie must survive top-level navigations.
        SameSite cookieSameSite = SameSite::Strict;
    };

    /**
     * @brief CSRF protection middleware using the double-submit-cookie
     *        pattern.
     *
     * On unsafe methods, requires the request header to equal the cookie
     * value. On safe methods, optionally issues a fresh cookie when one is
     * missing.
     */
    class CsrfMiddleware final : public IMiddleware
    {
      public:
        /**
         * @brief Construct the middleware with the given options.
         */
        explicit CsrfMiddleware(CsrfOptions options = {}) :
            mOptions(std::move(options))
        {
        }

        ~CsrfMiddleware() override = default;

        void Handle(HttpRequest&          request,
                    HttpResponse&         response,
                    const NextMiddleware& next) override
        {
            const bool unsafe =
                mOptions.protectedMethods.count(request.method) > 0;
            const bool exempt = isExempt(request.path);

            if (unsafe && !exempt)
            {
                const auto lowered = baldr::toLowerAscii(mOptions.headerName);
                auto headerIt     = request.headers.find(lowered);
                if (headerIt == request.headers.end() ||
                    headerIt->second.empty())
                {
                    reject(response, "Missing CSRF token");
                    return;
                }

                auto cookieIt = request.cookies.find(mOptions.cookieName);
                if (cookieIt == request.cookies.end() ||
                    cookieIt->second.empty())
                {
                    reject(response, "Missing CSRF cookie");
                    return;
                }

                if (!constantTimeEqual(headerIt->second, cookieIt->second))
                {
                    reject(response, "Invalid CSRF token");
                    return;
                }
            }
            else if (mOptions.issueCookieOnSafeRequest)
            {
                if (request.cookies.find(mOptions.cookieName) ==
                    request.cookies.end())
                {
                    std::string   token = generateToken();
                    CookieOptions opts;
                    opts.value     = token;
                    opts.httpOnly  = mOptions.cookieHttpOnly;
                    opts.secure    = mOptions.cookieSecure;
                    opts.maxAge    = mOptions.cookieMaxAge;
                    opts.sameSite  = mOptions.cookieSameSite;
                    response.cookies[mOptions.cookieName] = opts;
                }
            }

            next();
        }

      private:
        static bool constantTimeEqual(std::string_view a, std::string_view b)
        {
            if (a.size() != b.size())
                return false;
            unsigned char acc = 0;
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                acc |= static_cast<unsigned char>(
                    static_cast<unsigned char>(a[i]) ^
                    static_cast<unsigned char>(b[i]));
            }
            return acc == 0;
        }

        bool isExempt(const std::string& path) const
        {
            for (const auto& prefix : mOptions.exemptPathPrefixes)
            {
                if (path.size() >= prefix.size() &&
                    path.compare(0, prefix.size(), prefix) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        static std::string generateToken()
        {
            // 128 bits of entropy from std::random_device + mt19937_64;
            // sufficient for the double-submit pattern when the cookie
            // is bound to a session origin.
            return RandomHex(32);
        }

        void reject(HttpResponse& response, const std::string& message)
        {
            response.statusCode              = StatusCode::Forbidden;
            response.body                    = message;
            response.headers["Content-Type"] = "text/plain";
        }

        CsrfOptions mOptions;
    };

} // namespace BALDR_NAMESPACE
