#pragma once

#include <optional>
#include <string>

#include "IMiddleware.hpp"

struct SecurityHeadersOptions
{
    // X-Content-Type-Options: nosniff
    std::optional<std::string> contentTypeOptions = "nosniff";

    // X-Frame-Options: DENY | SAMEORIGIN | (empty to disable)
    std::optional<std::string> frameOptions = "DENY";

    // Referrer-Policy: no-referrer | same-origin |
    // strict-origin-when-cross-origin | ...
    std::optional<std::string> referrerPolicy =
        "strict-origin-when-cross-origin";

    // Strict-Transport-Security. Only emitted when this option is set.
    // Recommended: "max-age=31536000; includeSubDomains" when terminating
    // TLS upstream.
    std::optional<std::string> strictTransportSecurity = std::nullopt;

    // Permissions-Policy, e.g. "geolocation=(), microphone=()". Set to
    // empty string to disable.
    std::optional<std::string> permissionsPolicy = std::nullopt;

    // Cross-Origin-Opener-Policy: same-origin | same-origin-allow-popups
    std::optional<std::string> crossOriginOpenerPolicy = "same-origin";

    // Cross-Origin-Resource-Policy: same-origin | same-site | cross-origin
    std::optional<std::string> crossOriginResourcePolicy = "same-origin";

    // Cross-Origin-Embedder-Policy: require-corp | credentialless | ...
    std::optional<std::string> crossOriginEmbedderPolicy = std::nullopt;
};

class SecurityHeadersMiddleware final : public IMiddleware
{
  public:
    explicit SecurityHeadersMiddleware(SecurityHeadersOptions options = {}) :
        mOptions(std::move(options))
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
            response.headers["X-Content-Type-Options"] = *o.contentTypeOptions;
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
