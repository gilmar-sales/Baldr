#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <functional>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>

namespace BALDR_NAMESPACE {

using NextMiddleware = std::function<void()>;

class IMiddleware
{
  public:
    virtual ~IMiddleware() = default;

    virtual void Handle(HttpRequest&          request,
                        HttpResponse&         response,
                        const NextMiddleware& next) = 0;
};

} // namespace BALDR_NAMESPACE
