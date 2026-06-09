#pragma once

#include "Baldr/HttpRequest.hpp"
#include "Baldr/HttpResult.hpp"

class HttpRequestParser
{
  public:
    // Parse a full request (headers + body) contained entirely in `request`.
    HttpResult<HttpRequest> parse(const std::string& request);

    // Parse a request where the first `headerByteCount` bytes of `request`
    // are the header block (terminated by "\r\n\r\n") and the body follows
    // at offset `headerByteCount`. `request.size()` must be
    // `headerByteCount + bodyLen`; the parser verifies this and returns
    // a "request body incomplete" error otherwise, so the caller can read
    // more bytes and retry.
    HttpResult<HttpRequest> parse(const std::string& request,
                                  std::size_t        headerByteCount);
};
