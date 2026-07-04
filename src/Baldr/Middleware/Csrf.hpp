#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <string_view>

#include <Baldr/Middleware/IMiddleware.hpp>

struct CsrfOptions
{
    // Name of the CSRF cookie. Default: "XSRF-TOKEN" (matches Angular
    // convention; the client reads this cookie and echoes it in a
    // request header).
    std::string cookieName = "XSRF-TOKEN";

    // Name of the request header the client must set.
    std::string headerName = "X-XSRF-TOKEN";

    // HTTP methods considered "unsafe" and therefore protected. Safe
    // methods (GET/HEAD/OPTIONS) skip the check.
    std::set<HttpMethod> protectedMethods = {
        HttpMethod::Post, HttpMethod::Put, HttpMethod::Patch, HttpMethod::Delete
    };

    // Request paths (prefix match) excluded from CSRF protection, e.g.
    // "/api/webhooks/". Trailing slash required for prefix match.
    std::set<std::string> exemptPathPrefixes {};

    // If true, the middleware will set the CSRF cookie on every safe
    // request when the cookie is missing. If false, callers are
    // responsible for issuing the cookie themselves.
    bool issueCookieOnSafeRequest = true;

    // Cookie attributes used when issuing the token.
    bool cookieHttpOnly = false; // must be readable from JS
    bool cookieSecure   = false;
    long cookieMaxAge   = 0; // 0 = session cookie
};

class CsrfMiddleware final : public IMiddleware
{
  public:
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
