#pragma once

#include <string>
#include <unordered_map>

#include "HttpMethod.hpp"

struct HttpRequest
{
    HttpMethod                                   method;
    std::string                                  path;
    std::string                                  version;
    std::string                                  clientIp;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query;
    std::unordered_map<std::string, std::string> params;
    std::string                                  body;

    HttpRequest() = default;
};
