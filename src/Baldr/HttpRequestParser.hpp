#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "Baldr/HttpRequest.hpp"
#include "Baldr/HttpResult.hpp"

struct HttpParseStatus
{
    enum class Kind
    {
        Incomplete,
        Complete,
        Error,
    };

    Kind          kind = Kind::Incomplete;
    HttpRequest   request;
    std::string   errorMessage;
    StatusCode    statusCode = StatusCode::OK;
    std::size_t   consumedBytes = 0;
};

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

    // Incremental parse: try to extract a complete HTTP/1.1 request from
    // `buffer`. Returns Incomplete if the buffer does not yet contain a full
    // request (need more bytes), Complete with the parsed request and the
    // number of bytes consumed, or Error with the reason.
    HttpParseStatus tryParse(std::string_view buffer) const;
};
