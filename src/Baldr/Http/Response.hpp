/**
 * @file Http/Response.hpp
 * @brief Mutable response object handed to handlers and middleware.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <memory>

#include <Baldr/Http/CookieOptions.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE {

class IStreamingResult;

/**
 * @brief Mutable response built up by handlers and middleware.
 *
 * Handlers may populate @c body/@c headers/@c cookies directly, return an
 * @ref IResult (which calls @ref Apply), or return an
 * @ref IStreamingResult (stored in @ref streaming for chunked delivery
 * by the connection layer).
 */
struct HttpResponse
{
    HttpResponse() = default;

    /**
     * @brief Construct a default response matching the request's HTTP
     *        version with status 404 (Not Found).
     */
    HttpResponse(const HttpRequest& request)
    {
        version    = request.version;
        statusCode = StatusCode::NotFound;
        headers    = {};
        cookies    = {};
        body       = {};
    }

    std::string                                    version;       ///< HTTP version string.
    StatusCode                                     statusCode;    ///< Status code to send.
    std::unordered_map<std::string, std::string>   headers;       ///< Outgoing headers (lowercase keys).
    std::unordered_map<std::string, CookieOptions> cookies;       ///< Cookies to serialise as @c Set-Cookie.
    std::string                                    body;          ///< Buffered response body (empty when streaming).

    /**
     * @brief When set, the framework writes this streaming result with
     *        chunked transfer encoding instead of buffering @c body.
     *
     * Handlers populate it indirectly by returning an
     * @ref IStreamingResult; @ref IResult subclasses go through @ref Apply.
     */
    std::shared_ptr<const IStreamingResult> streaming;
};

} // namespace BALDR_NAMESPACE
