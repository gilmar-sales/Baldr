#pragma once

#include <cstddef>
#include <string>

#include <zlib.h>

namespace Baldr::Detail
{
    // Compress `input` using gzip-encoded DEFLATE. Returns true on
    // success and writes the compressed bytes to `output`. `output` is
    // cleared on failure.
    bool gzipCompress(std::string_view input, std::string& output,
                      int level = Z_DEFAULT_COMPRESSION);

    // Decompress `input` (gzip-encoded DEFLATE) to `output`. Used in
    // tests; not invoked on the request path because Baldr only emits
    // compressed responses (clients send already-compressed request
    // bodies when applicable).
    bool gzipDecompress(std::string_view input, std::string& output);
}
