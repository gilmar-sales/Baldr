#pragma once

#include <string>
#include <unordered_set>

#include "IMiddleware.hpp"

struct CompressionOptions
{
    // Only compress responses whose Content-Type starts with one of
    // these prefixes (case-insensitive). Defaults to text-like types.
    std::unordered_set<std::string> mimeTypePrefixes = {
        "text/",
        "application/json",
        "application/javascript",
        "application/xml",
        "application/xhtml+xml",
        "image/svg+xml",
    };

    // Skip responses smaller than this many bytes. Defaults to 1024
    // (compression overhead rarely pays off on tiny bodies).
    std::size_t minBodyBytes = 1024;

    // Compression level passed to zlib (1..9, default 6). -1 selects
    // zlib's default.
    int level = -1;
};

class CompressionMiddleware final : public IMiddleware
{
  public:
    explicit CompressionMiddleware(CompressionOptions options = {}) :
        mOptions(std::move(options))
    {
    }

    ~CompressionMiddleware() override = default;

    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override;

  private:
    static std::string toLowerAscii(std::string_view s);

    static bool mimeAllowed(const std::string&                      ctype,
                            const std::unordered_set<std::string>& prefixes);

  public:
    // Public helper for unit tests; not part of the framework's
    // user-facing API.
    static bool mimeAllowedForTest(
        const std::string&                      ctype,
        const std::unordered_set<std::string>& prefixes)
    {
        return mimeAllowed(ctype, prefixes);
    }

    CompressionOptions mOptions;
};
