# Middleware

Middleware lets you run code before and after every request — perfect for logging, authentication, rate limiting, request transformation, and cross-cutting security headers.

## The `IMiddleware` interface

Every middleware implements the [`IMiddleware`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/IMiddleware.hpp) interface defined in [`src/Baldr/Middleware/IMiddleware.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/IMiddleware.hpp):

```cpp title="IMiddleware.hpp"
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
```

A middleware:

1. Inspects or mutates the `request` / `response`.
2. Calls `next()` to invoke the next middleware or the route handler.
3. Optionally inspects or mutates the `response` after `next()` returns.

If `next()` is not called, the request is short-circuited — no further middleware or handler runs.

`HttpRequest` is passed by non-const reference, so middleware may attach per-request context (for example `RequestIdMiddleware` echoes the id on both the request and the response headers).

## Registering middleware

Middleware is added to a `WebApplication` via the `Use<T>()` template, in the order you want it to run:

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->Use<LoggingMiddleware>();

    app->MapGet("/", [] { return Payload { .message = "ok" }; });

    app->Run();
}
```

All built-in middleware headers are pulled in via the umbrella `<Baldr/Baldr.hpp>`. Individual headers live under `<Baldr/Middleware/...>` (for example `<Baldr/Middleware/Logging.hpp>`).

`Use<T>()` registers the middleware with the framework's middleware provider. It returns `const WebApplication&`, so call sites that need to chain further configuration use `.Use<...>()` (not `->`). The middleware is resolved from the service provider on every request, so it can take constructor dependencies (for example `RateLimitMiddleware` requires a `skr::Arc<RateLimiter>`).

## Built-in middleware

Baldr ships with the following middleware out of the box:

| Middleware | Header | Purpose |
| --- | --- | --- |
| `LoggingMiddleware` | `<Baldr/Middleware/Logging.hpp>` | Logs request/response with elapsed microseconds. |
| `RequestIdMiddleware` | `<Baldr/Middleware/RequestId.hpp>` | Echoes or generates `X-Request-ID`; parses W3C `traceparent` and exposes `request.traceContext`. |
| `ExceptionHandlerMiddleware` | `<Baldr/Middleware/ExceptionHandler.hpp>` | Catches exceptions and maps them to a 500 response. |
| `RateLimitMiddleware` | `<Baldr/Middleware/RateLimit/Middleware.hpp>` | Per-client throttling backed by `RateLimiter`. |
| `CorsMiddleware` | `<Baldr/Middleware/Cors.hpp>` | CORS headers + `OPTIONS` preflight short-circuit. |
| `CsrfMiddleware` | `<Baldr/Middleware/Csrf.hpp>` | Double-submit cookie CSRF protection. |
| `SecurityHeadersMiddleware` | `<Baldr/Middleware/SecurityHeaders.hpp>` | Sets X-Content-Type-Options, X-Frame-Options, HSTS, COOP/CORP, etc. |
| `CompressionMiddleware` | `<Baldr/Middleware/Compression/Middleware.hpp>` | gzip-encodes eligible response bodies. |

See [Middleware overview](../middleware/overview.md) for per-middleware pages with options and examples.

Example wiring:

```cpp title="src/main.cpp"
app.Use<RequestIdMiddleware>()
   .Use<ExceptionHandlerMiddleware>()
   .Use<LoggingMiddleware>()
   .Use<CompressionMiddleware>()
   .Use<SecurityHeadersMiddleware>()
   .Use<CorsMiddleware>()
   .Use<RateLimitMiddleware>();
```

## Writing your own

Implement `IMiddleware`, register the implementation in the service collection, and add it with `Use<T>()`. Because middleware is resolved from the service provider, you can inject loggers, configuration, and any other registered services.

```cpp title="TimingMiddleware.hpp"
#pragma once

#include <Baldr/Middleware/IMiddleware.hpp>

class TimingMiddleware final : public IMiddleware
{
  public:
    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override
    {
        (void) request;
        auto begin = std::chrono::steady_clock::now();
        next();
        auto end = std::chrono::steady_clock::now();

        response.headers["X-Elapsed-Us"] =
            std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                               end - begin)
                               .count());
    }
};
```

Register it on the builder alongside `BaldrExtension`:

```cpp title="src/main.cpp"
builder.GetServiceCollection()->AddTransient<TimingMiddleware>();
app->Use<TimingMiddleware>();
```

## Order of execution

Middleware runs in registration order on the way **in**, and in reverse order on the way **out**. Register cross-cutting concerns (logging, request IDs, exception handling) first, and request-specific concerns (auth, rate limiting, CORS) closer to the handler.

Typical order, outermost first:

1. `RequestIdMiddleware` — assigns an id before anything else so it appears in every log. Also parses the W3C `traceparent` header and populates `request.traceContext`, so it must run **before** `LoggingMiddleware` for log enrichment to work.
2. `ExceptionHandlerMiddleware` — wraps the rest of the pipeline so uncaught exceptions become 500s.
3. `LoggingMiddleware` — measures elapsed time including everything below. Reads `request.traceContext` to append `trace=` (and `span=` when sampled) to each log line. See [Tracing](tracing.md).
4. `CompressionMiddleware` — only relevant for response bodies, so position relative to `LoggingMiddleware` does not matter for ordering correctness.
5. `SecurityHeadersMiddleware` — adds headers regardless of response.
6. `CorsMiddleware` — must run before the handler to short-circuit preflight `OPTIONS`.
7. `RateLimitMiddleware` — close to the handler so rejected requests still benefit from logging and request-id propagation.

## Next steps

- Browse per-middleware pages under [Middleware overview](../middleware/overview.md).
- See runnable programs that combine middleware in the [Examples walkthrough](../authoring/examples.md).
- Configure distributed tracing with [Tracing](tracing.md).