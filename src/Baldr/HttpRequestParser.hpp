#pragma once

#include "Baldr/HttpRequest.hpp"
#include "Baldr/HttpResult.hpp"

class HttpRequestParser
{
  public:
    HttpResult<HttpRequest> parse(const std::string& request);
};
