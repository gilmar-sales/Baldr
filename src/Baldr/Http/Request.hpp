#pragma once

#include <string>
#include <unordered_map>

#include <Baldr/Http/CookieOptions.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/RouteOptions.hpp>
#include <Baldr/Http/TraceContext.hpp>

struct HttpRequest
{
    HttpMethod                                   method;
    std::string                                  path;
    std::string                                  version;
    std::string                                  clientIp;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query;
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> cookies;
    std::string                                  body;
    Baldr::RouteInfo                             route;
    Baldr::TraceContext                          traceContext;

    HttpRequest() = default;
};
