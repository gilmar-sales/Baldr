/**
 * @file Http/StaticFilesInternal.hpp
 * @brief Helpers used by the static-file handler (path resolution, ETag
 *        and HTTP-date formatting/parsing). Internal API.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE
{

    namespace Detail
    {
        /**
         * @brief Outcome of resolving a request path against the static
         *        file root.
         */
        struct StaticResolve
        {
            StatusCode status; ///< Suggested HTTP status (200/403/404).
            std::filesystem::path canonical; ///< Canonicalised absolute path of
                                             ///< the resolved file.
            std::string mimeType; ///< Best-effort MIME type based on extension.
            std::string body;     ///< Inline body for error responses.

            /// File size in bytes. Only meaningful when @c status == OK.
            std::uintmax_t fileSize = 0;
            /// Last-modified timestamp. Only meaningful when @c status == OK.
            std::chrono::system_clock::time_point lastModified {};

            /// ETag value (quoted, e.g. @c "<size>-<mtimeHex>"). Empty if not
            /// OK.
            std::string etag;
        };

        /**
         * @brief Resolve @p filepath against the safe root and return the
         *        resolved file plus metadata.
         *
         * Performs canonicalisation and rejects paths that escape @p root
         * (returning @c Forbidden).
         */
        StaticResolve resolveStaticFile(const std::string& filepath,
                                        const std::string& root);

        /**
         * @brief Build a strong ETag of the form @c "<size>-<mtime>".
         *
         * Timestamp resolution is per-second (mtime granularity).
         */
        std::string makeEtag(std::uintmax_t                        size,
                             std::chrono::system_clock::time_point mtime);

        /**
         * @brief Format an HTTP-date (RFC 7231 IMF-fixdate) for use in a
         *        @c Last-Modified response header.
         */
        std::string formatHttpDate(std::chrono::system_clock::time_point tp);

        /**
         * @brief Parse an HTTP-date (RFC 7231 IMF-fixdate, RFC 850,
         *        @c asctime). Returns @c time_t::max() on failure.
         */
        std::chrono::system_clock::time_point parseHttpDate(std::string_view v);
    } // namespace Detail

} // namespace BALDR_NAMESPACE
