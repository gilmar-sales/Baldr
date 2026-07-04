/**
 * @file Middleware/IMiddleware.hpp
 * @brief Core middleware interface implemented by every interceptor in
 *        the Baldr request pipeline.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <functional>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>

namespace BALDR_NAMESPACE {

/**
 * @brief Callable invoked by a middleware to continue down the chain.
 *
 * Implementations of @ref IMiddleware::Handle should call @c next exactly
 * once unless they intentionally short-circuit the chain (e.g. by
 * populating the response and returning without calling @c next).
 */
using NextMiddleware = std::function<void()>;

/**
 * @brief Contract for an HTTP request interceptor.
 *
 * Middleware execute in registration order. They can mutate the
 * request, the response, or both; the chain reaches the route handler
 * when the last middleware's @c next is invoked.
 */
class IMiddleware
{
  public:
    virtual ~IMiddleware() = default;

    /**
     * @brief Process the in-flight request/response pair.
     *
     * @param request   Mutable request. Middleware may attach context
     *                  that downstream consumers will see.
     * @param response  Mutable response. Middleware may pre-populate
     *                  headers/body or react to whatever the handler
     *                  wrote.
     * @param next      Callable that resumes the chain. Calling it is
     *                  required unless the middleware short-circuits.
     */
    virtual void Handle(HttpRequest&          request,
                        HttpResponse&         response,
                        const NextMiddleware& next) = 0;
};

} // namespace BALDR_NAMESPACE
