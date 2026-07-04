/**
 * @file Middleware/Compression/Internal.hpp
 * @brief zlib-backed gzip compression / decompression helpers used by
 *        the compression middleware.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <cstddef>
#include <string>

#include <zlib.h>

namespace BALDR_NAMESPACE
{

    namespace Detail
    {
        /**
         * @brief Compress @p input using gzip-encoded DEFLATE.
         *
         * @param input  Bytes to compress.
         * @param output Receives the compressed bytes on success; cleared
         *               on failure.
         * @param level  zlib compression level (1..9, or @c Z_DEFAULT_COMPRESSION).
         * @return @c true on success.
         */
        bool gzipCompress(std::string_view input, std::string& output,
                          int level = Z_DEFAULT_COMPRESSION);

        /**
         * @brief Decompress @p input (gzip-encoded DEFLATE) to @p output.
         *
         * Used in tests; not invoked on the request path because Baldr
         * only emits compressed responses (clients send already-compressed
         * request bodies when applicable).
         */
        bool gzipDecompress(std::string_view input, std::string& output);
    } // namespace Detail

} // namespace BALDR_NAMESPACE
