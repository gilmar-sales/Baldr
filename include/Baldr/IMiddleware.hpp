#pragma once

#include <functional>

#include <Skirnir/ServiceProvider.hpp>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using NextMiddleware = std::function<void()>;

class IMiddleware
{
  public:
    virtual ~IMiddleware() = default;

    virtual void Handle(const HttpRequest& request, HttpResponse& response,
                        NextMiddleware& next) = 0;
};

using MiddlewareFactory =
    std::function<Ref<IMiddleware>(const Ref<skr::ServiceProvider>&)>;
using MiddlewareFactoryList = std::vector<MiddlewareFactory>;