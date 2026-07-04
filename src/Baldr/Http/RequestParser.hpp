#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Results/HttpResult.hpp>

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
    HttpRequestParser() = default;

    // Maximum allowed Content-Length / body size in bytes.
    std::size_t maxBodySize = 100 * 1024 * 1024;

    // Incremental parse: try to extract a complete HTTP/1.1 request from
    // `buffer`. Returns Incomplete if the buffer does not yet contain a full
    // request (need more bytes), Complete with the parsed request and the
    // number of bytes consumed, or Error with the reason.
    HttpParseStatus tryParse(std::string_view buffer) const;
};
