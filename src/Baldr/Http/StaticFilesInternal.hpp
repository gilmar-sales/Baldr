#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

#include <Baldr/Http/StatusCode.hpp>

namespace Baldr::Detail
{
    struct StaticResolve
    {
        StatusCode            status;
        std::filesystem::path canonical;
        std::string           mimeType;
        std::string           body;

        // File metadata. Only meaningful when `status == OK`.
        std::uintmax_t                        fileSize = 0;
        std::chrono::system_clock::time_point lastModified {};

        // ETag value (quoted, e.g. `"<size>-<mtimeHex>"`). Empty if not OK.
        std::string etag;
    };

    StaticResolve resolveStaticFile(const std::string& filepath,
                                    const std::string& root);

    // Build a strong ETag of the form "<size>-<mtime>" wrapped in quotes.
    // Timestamp resolution is per-second (mtime granularity).
    std::string makeEtag(std::uintmax_t                        size,
                         std::chrono::system_clock::time_point mtime);

    // Format an HTTP-date Last-Modified header (RFC 7231 IMF-fixdate).
    std::string formatHttpDate(std::chrono::system_clock::time_point tp);

    // Parse an HTTP-date (RFC 7231 IMF-fixdate, RFC 850, asctime).
    // Returns time_t::max() on failure.
    std::chrono::system_clock::time_point parseHttpDate(std::string_view v);
} // namespace Baldr::Detail
