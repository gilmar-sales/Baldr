# Middleware

Middleware lets you run code before and after every request — perfect for logging, authentication, rate limiting, and request transformation.

## The `IMiddleware` interface

Every middleware implements the [`IMiddleware`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/IMiddleware.hpp) interface:

```cpp title="IMiddleware.hpp"
#pragma once

#include <functional>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using NextMiddleware = std::function<void()>;

class IMiddleware
{
  public:
    virtual ~IMiddleware() = default;

    virtual void Handle(const HttpRequest&    request,
                        HttpResponse&         response,
                        const NextMiddleware& next) = 0;
};
```

A middleware:

1. Inspects or mutates the `request` / `response`.
2. Calls `next()` to invoke the next middleware or the route handler.
3. Optionally inspects or mutates the `response` after `next()` returns.

If `next()` is not called, the request is short-circuited — no further middleware or handler runs.

## Registering middleware

Middleware is added to a `WebApplication` via the `Use<T>()` template, in the order you want it to run:

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>
#include <Baldr/LoggingMiddleware.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->Use<LoggingMiddleware>();

    app->MapGet("/", [] { return Payload { .message = "ok" }; });

    app->Run();
}
```

`Use<T>()` registers the middleware with the `MiddlewareProvider` (see [`src/Baldr/MiddlewareProvider.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/MiddlewareProvider.hpp)). The middleware is resolved from the service provider on every request, so it can take constructor dependencies.

## Built-in middleware

Baldr ships with two ready-to-use middleware:

- **`LoggingMiddleware`** — logs each request and response with timing information. See [Logging middleware](../extensions/logging.md).
- **`RateLimitMiddleware`** — rejects requests from clients that exceed a configurable rate. See [Rate-limit middleware](../extensions/rate-limit.md).

## Writing your own

Implement `IMiddleware`, register the implementation in the service collection, and add it with `Use<T>()`. Because middleware is resolved from the service provider, you can inject loggers, configuration, and any other registered services.

```cpp title="TimingMiddleware.hpp"
#pragma once

#include "IMiddleware.hpp"

class TimingMiddleware final : public IMiddleware
{
  public:
    void Handle(const HttpRequest& request,
                HttpResponse& response,
                const NextMiddleware& next) override
    {
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

Middleware runs in registration order on the way **in**, and in reverse order on the way **out**. Register cross-cutting concerns (logging, request IDs) first, and request-specific concerns (auth, rate limiting) closer to the handler.

## Next steps

- See a complete example that uses both built-in middleware in the [Examples walkthrough](../authoring/examples.md).