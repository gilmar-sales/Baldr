/**
 * @file Http/Request.hpp
 * @brief Plain-data representation of an HTTP request passed to handlers.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>
#include <unordered_map>

#include <Baldr/Http/CookieOptions.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/RouteOptions.hpp>
#include <Baldr/Http/TraceContext.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Parsed HTTP/1.1 request handed to route handlers.
     *
     * Created by @c HttpRequestParser and mutated by middleware along the
     * pipeline. The maps are all owned by the request and use lowercase
     * keys for headers (HTTP/2-style), preserving the original casing only
     * in the wire form.
     */
    struct HttpRequest
    {
        HttpMethod                                   method;       ///< Request method.
        std::string                                  path;         ///< Path component (no query string).
        std::string                                  version;      ///< HTTP version string (e.g. @c "HTTP/1.1").
        std::string                                  clientIp;     ///< Remote peer address.
        std::unordered_map<std::string, std::string> headers;      ///< Header map (lowercase keys).
        std::unordered_map<std::string, std::string> query;        ///< Decoded query-string parameters.
        std::unordered_map<std::string, std::string> params;       ///< Path-template parameters (e.g. @c :id).
        std::unordered_map<std::string, std::string> cookies;      ///< Decoded cookies from the @c Cookie header.
        std::string                                  body;         ///< Raw request body.
        RouteInfo                                    route;        ///< Resolved route identity.
        TraceContext                                 traceContext; ///< W3C Trace Context state.

        HttpRequest() = default;
    };

} // namespace BALDR_NAMESPACE
