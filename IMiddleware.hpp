#pragma once

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class IMiddleware {
    public:
    virtual ~IMiddleware() = default;

    virtual void Handle(const HttpRequest& request, const HttpResponse& response) = 0;
};

using MiddlewareFactory = std::function<std::shared_ptr<IMiddleware>(const std::shared_ptr<ServiceProvider> &)>;
using MiddlewareFactoryList = std::vector<MiddlewareFactory>;