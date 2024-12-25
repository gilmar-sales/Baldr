#pragma once

#include "CookieOptions.hpp"
#include "HttpRequest.hpp"

struct HttpResponse {
    explicit HttpResponse(const HttpRequest &request) {
        version = request.version;
        statusCode = 200;
        headers = {};
        cookies = {};
        body = {};
    }

    std::string version;
    unsigned short statusCode;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, CookieOptions> cookies;
    std::string body;
};
