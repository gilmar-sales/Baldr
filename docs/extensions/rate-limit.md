# Rate-limit middleware

`RateLimitMiddleware` rejects requests from clients that exceed a configurable rate. It is a thin wrapper around the `RateLimiter` token-bucket implementation in [`src/Baldr/RateLimiter.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/RateLimiter.hpp).

## How it works

Each client is identified by its IP address. When a request arrives:

1. The middleware asks `RateLimiter::isAllowed(clientIp)` whether the client has tokens remaining.
2. If yes, the request continues to the next middleware or handler.
3. If no, the middleware sets the response status to `429 Too Many Requests` and returns a fixed JSON body without calling `next()`.

`RateLimitMiddleware` is defined in [`src/Baldr/RateLimitMiddleware.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/RateLimitMiddleware.hpp).

## Enabling it

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>
#include <Baldr/RateLimiter.hpp>
#include <Baldr/RateLimitMiddleware.hpp>

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

The example above configures a `RateLimiter` that allows **100 requests per 60 seconds** per client. Adjust the two constructor arguments to suit your workload.

## Response on rejection

When the limit is exceeded, the middleware returns:

```http
HTTP/1.1 429 Too Many Requests
Content-Type: application/json
Content-Length: 45

{ "status": 429, "message": "Too Many Requests" }
```

## Where to put it

Register `RateLimitMiddleware` **after** `LoggingMiddleware` so the rejected responses are still logged:

```cpp title="src/main.cpp"
app->Use<LoggingMiddleware>();
app->Use<RateLimitMiddleware>();
```