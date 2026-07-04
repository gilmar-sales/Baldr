/**
 * @file Middleware/ExceptionHandler.hpp
 * @brief Middleware that converts thrown exceptions into a generic 500
 *        response, optionally with a custom mapper.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <exception>
#include <functional>
#include <string>
#include <utility>

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE {

/**
 * @brief Configuration for @ref ExceptionHandlerMiddleware.
 */
struct ExceptionHandlerOptions
{
    /**
     * @brief Custom mapper that converts a caught @c std::exception into
     *        a response body string.
     *
     * When @ref includeDetailsInDev is @c false (the default) the
     * built-in mapper emits a generic body so internal exception
     * messages do not leak to clients; the mapper still receives the
     * exception so it can decide locally whether to log the details.
     */
    std::function<std::string(const std::exception&)> mapper;

    /**
     * @brief When @c true, the built-in mapper falls back to @c e.what().
     *        Disable in release builds to avoid information leakage.
     */
    bool includeDetailsInDev = false;

    /// Content-Type used for the generated response.
    std::string contentType = "text/plain";

    /// Status emitted on any caught exception (typed or non-typed).
    StatusCode status = StatusCode::InternalServerError;
};

/**
 * @brief Middleware that converts exceptions thrown further down the
 *        chain into an HTTP 500 response.
 */
class ExceptionHandlerMiddleware final : public IMiddleware
{
  public:
    /**
     * @brief Construct the middleware with the given options.
     */
    explicit ExceptionHandlerMiddleware(ExceptionHandlerOptions options = {}) :
        mOptions(std::move(options))
    {
        if (!mOptions.mapper)
        {
            mOptions.mapper = [includeDetails = mOptions.includeDetailsInDev](
                                  const std::exception& e) {
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
        (void) request;
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
            response.body                    = mOptions.includeDetailsInDev
                                                   ? "Unknown error"
                                                   : "Internal Server Error";
            response.headers["Content-Type"] = mOptions.contentType;
        }
    }

  private:
    ExceptionHandlerOptions mOptions;
};

} // namespace BALDR_NAMESPACE
