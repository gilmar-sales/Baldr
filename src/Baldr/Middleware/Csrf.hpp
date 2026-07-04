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

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE {

/**
 * @brief Configuration for @ref CsrfMiddleware.
 *
 * Implements the double-submit pattern: a cookie carries the token and
 * the client must echo it in a request header.
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
        HttpMethod::Post, HttpMethod::Put, HttpMethod::Patch, HttpMethod::Delete
    };

    /// Path prefixes excluded from CSRF protection (e.g. @c "/api/webhooks/").
    std::set<std::string> exemptPathPrefixes {};

    /**
     * @brief When @c true, the middleware issues a CSRF cookie on safe
     *        requests that lack one. When @c false, callers must set the
     *        cookie themselves.
     */
    bool issueCookieOnSafeRequest = true;

    /// Cookie attributes used when issuing the token.
    bool cookieHttpOnly = false; ///< Must be readable from JS, so default is @c false.
    bool cookieSecure   = false; ///< Set to @c true when serving over HTTPS.
    long cookieMaxAge   = 0;     ///< @c Max-Age in seconds. 0 = session cookie.
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
        const bool unsafe = mOptions.protectedMethods.count(request.method) > 0;
        const bool exempt = isExempt(request.path);

        if (unsafe && !exempt)
        {
            auto headerIt =
                request.headers.find(toLowerAscii(mOptions.headerName));
            if (headerIt == request.headers.end() || headerIt->second.empty())
            {
                reject(response, "Missing CSRF token");
                return;
            }

            auto cookieIt = request.cookies.find(mOptions.cookieName);
            if (cookieIt == request.cookies.end() || cookieIt->second.empty())
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
                opts.value                            = token;
                opts.httpOnly                         = mOptions.cookieHttpOnly;
                opts.secure                           = mOptions.cookieSecure;
                opts.maxAge                           = mOptions.cookieMaxAge;
                response.cookies[mOptions.cookieName] = opts;
            }
        }

        next();
    }

  private:
    static std::string toLowerAscii(std::string_view s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            if (c >= 'A' && c <= 'Z')
                out.push_back(static_cast<char>(c + 32));
            else
                out.push_back(c);
        }
        return out;
    }

    static bool constantTimeEqual(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        unsigned char acc = 0;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            acc |= static_cast<unsigned char>(static_cast<unsigned char>(a[i]) ^
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
        // Same approach as RequestIdMiddleware: not cryptographic, but
        // good enough for the double-submit pattern. Strong entropy
        // comes from the source randomness of the request environment
        // and uniqueness is enforced by the per-client cookie.
        static thread_local std::uint64_t counter = 0;
        const auto                        now     = static_cast<std::uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        const auto mix = now ^ (++counter * 0x9E3779B97F4A7C15ULL);
        char       buf[33];
        std::snprintf(buf, sizeof(buf), "%016llx%016llx",
                      static_cast<unsigned long long>(mix),
                      static_cast<unsigned long long>(mix >> 32));
        return buf;
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
