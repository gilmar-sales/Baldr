#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <memory>

#include <Baldr/Http/CookieOptions.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE {

class IStreamingResult;

struct HttpResponse
{
    HttpResponse() = default;

    HttpResponse(const HttpRequest& request)
    {
        version    = request.version;
        statusCode = StatusCode::NotFound;
        headers    = {};
        cookies    = {};
        body       = {};
    }

    std::string                                    version;
    StatusCode                                     statusCode;
    std::unordered_map<std::string, std::string>   headers;
    std::unordered_map<std::string, CookieOptions> cookies;
    std::string                                    body;

    // When a handler returns an IStreamingResult, MapRoute stores it
    // here. HttpConnection::handle checks for this and bypasses the
    // buffered body path to write the response directly to the socket
    // (e.g. with chunked transfer-encoding).
    std::shared_ptr<const IStreamingResult> streaming;
};

} // namespace BALDR_NAMESPACE
