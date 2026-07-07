# Rate-limit middleware

`RateLimitMiddleware` rejects requests from clients that exceed a configurable rate. It is a thin wrapper around the `RateLimiter` token-bucket implementation in [`src/Baldr/Middleware/RateLimit/Limiter.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/RateLimit/Limiter.hpp).

## How it works

Each client is identified by its IP address. When a request arrives:

1. The middleware asks `RateLimiter::isAllowed(clientIp)` whether the client has tokens remaining.
2. If yes, the request continues to the next middleware or handler.
3. If no, the middleware sets the response status to `429 Too Many Requests`, writes a fixed JSON body, and returns without calling `next()`.

`RateLimitMiddleware` is defined in [`src/Baldr/Middleware/RateLimit/Middleware.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/RateLimit/Middleware.hpp).

## Enabling it

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();

    builder.GetServiceCollection()->AddSingleton<RateLimiter>(
        [] { return skr::Arc<RateLimiter>(
                  new RateLimiter(100, std::chrono::seconds(60))); });
    builder.GetServiceCollection()->AddTransient<RateLimitMiddleware>();

    auto app = builder.Build<WebApplication>();

    app->Use<RateLimitMiddleware>();

    app->MapGet("/", [] { return Payload { .message = "ok" }; });

    app->Run();
}
```

The example above configures a `RateLimiter` that allows **100 requests per 60 seconds** per client. Adjust the two constructor arguments to suit your workload. `RateLimiter` also accepts a third argument — the maximum number of tracked clients before the LRU starts evicting entries (default `10000`).

## Response on rejection

When the limit is exceeded, the middleware returns:

```http
HTTP/1.1 429 Too Many Requests
Content-Type: application/json

{ "status": 429, "message": "Too Many Requests" }
```

A warning is also logged via the injected `skr::Logger<RateLimitMiddleware>`.

## Where to put it

Register `RateLimitMiddleware` **after** `LoggingMiddleware` so the rejected responses are still logged:

```cpp title="src/main.cpp"
app->Use<LoggingMiddleware>();
app->Use<RateLimitMiddleware>();
```

Placing it later in the pipeline (closer to the handler) ensures cross-cutting middleware such as request IDs and logging still run for rejected requests.