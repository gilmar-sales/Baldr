#pragma once

#include <Skirnir/Skirnir.hpp>

#include <functional>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using NextMiddleware = std::function<void()>;

class IMiddleware
{
  public:
    virtual ~IMiddleware() = default;

    virtual void Handle(const HttpRequest& request, HttpResponse& response,
                        const NextMiddleware& next) = 0;
};