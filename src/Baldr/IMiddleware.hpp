#pragma once

#include <Skirnir/Skirnir.hpp>

#include <functional>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using NextMiddleware = std::function<skr::Task<>()>;

class IMiddleware
{
  public:
    virtual ~IMiddleware() = default;

    virtual skr::Task<> Handle(const HttpRequest&    request,
                               HttpResponse&         response,
                               const NextMiddleware& next) = 0;
};