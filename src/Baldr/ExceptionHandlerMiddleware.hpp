#pragma once

#include <exception>
#include <string>

#include "IMiddleware.hpp"

class ExceptionHandlerMiddleware final : public IMiddleware
{
  public:
    ExceptionHandlerMiddleware()           = default;
    ~ExceptionHandlerMiddleware() override = default;

    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override
    {
        try
        {
            next();
        }
        catch (const std::exception& e)
        {
            response.statusCode              = StatusCode::InternalServerError;
            response.body                    = e.what();
            response.headers["Content-Type"] = "text/plain";
        }
        catch (...)
        {
            response.statusCode              = StatusCode::InternalServerError;
            response.body                    = "Unknown error";
            response.headers["Content-Type"] = "text/plain";
        }
    }
};