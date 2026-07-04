#pragma once

#include <Skirnir/Skirnir.hpp>

#include <functional>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>

using NextMiddleware = std::function<void()>;

class IMiddleware
{
  public:
    virtual ~IMiddleware() = default;

    virtual void Handle(HttpRequest&          request,
                        HttpResponse&         response,
                        const NextMiddleware& next) = 0;
};
