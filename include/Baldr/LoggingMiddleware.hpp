#pragma once
#include "IMiddleware.hpp"

#include <iostream>
#include <print>

class LoggingMiddleware final : public IMiddleware
{
  public:
    void Handle(const HttpRequest& request, HttpResponse& response,
                NextMiddleware& next) override
    {
        std::cout << "Request received: " << request.version << " "
                  << request.method << " " << request.path << std::endl;

        std::print("Request received: {} {} {}\n", request.version,
                   request.method, request.path);

        next();
    }

    ~LoggingMiddleware() override
    {
        std::cout << "Logging finished" << std::endl;
    }
};