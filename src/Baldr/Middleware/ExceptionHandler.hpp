#pragma once

#include <exception>
#include <functional>
#include <string>
#include <utility>

#include <Baldr/Middleware/IMiddleware.hpp>

struct ExceptionHandlerOptions
{
    // Custom mapper that converts a caught std::exception into a
    // response payload. When `includeDetailsInDev` is `false` (the
    // default) the framework emits a generic 500 body so internal
    // exception messages do not leak to clients; the mapper still
    // receives the exception so it can decide locally whether to log
    // the details or take other action.
    std::function<std::string(const std::exception&)> mapper;

    // When true, the built-in mapper falls back to `e.what()`.
    // Disable this in release builds to avoid information leakage.
    bool includeDetailsInDev = false;

    // Content-Type used for the generated plain-text response.
    std::string contentType = "text/plain";

    // Status emitted on any caught exception (typed or non-typed).
    StatusCode status = StatusCode::InternalServerError;
};

class ExceptionHandlerMiddleware final : public IMiddleware
{
  public:
    explicit ExceptionHandlerMiddleware(ExceptionHandlerOptions options = {}) :
        mOptions(std::move(options))
    {
        if (!mOptions.mapper)
        {
            mOptions.mapper =
                [includeDetails =
                     mOptions.includeDetailsInDev](const std::exception& e) {
                    if (includeDetails)
                        return std::string(e.what());
                    return std::string("Internal Server Error");
                };
        }
    }

    ~ExceptionHandlerMiddleware() override = default;

    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override
    {
        (void)request;
        try
        {
            next();
        }
        catch (const std::exception& e)
        {
            response.statusCode              = mOptions.status;
            response.body                    = mOptions.mapper(e);
            response.headers["Content-Type"] = mOptions.contentType;
        }
        catch (...)
        {
            response.statusCode              = mOptions.status;
            response.body =
                mOptions.includeDetailsInDev
                    ? "Unknown error"
                    : "Internal Server Error";
            response.headers["Content-Type"] = mOptions.contentType;
        }
    }

  private:
    ExceptionHandlerOptions mOptions;
};
