#pragma once

#include "CookieOptions.hpp"
#include "HttpRequest.hpp"
#include "StatusCode.hpp"

struct HttpResponse {
    explicit HttpResponse(const HttpRequest &request) {
        version = request.version;
        statusCode = StatusCode::NotFound;
        headers = {};
        cookies = {};
        body = {};
    }

    std::string version;
    StatusCode statusCode;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, CookieOptions> cookies;
    std::string body;
};
