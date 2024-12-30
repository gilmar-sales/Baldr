#pragma once

#include <functional>

#include <ServiceProvider.hpp>

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

using MiddlewareFactory     = std::function<std::shared_ptr<IMiddleware>(
    const std::shared_ptr<ServiceProvider>&)>;
using MiddlewareFactoryList = std::vector<MiddlewareFactory>;