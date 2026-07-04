# CORS middleware

`CorsMiddleware` adds the standard CORS response headers and short-circuits `OPTIONS` preflight requests.

## Enabling it

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->Use<CorsMiddleware>();

    app->MapGet("/", [] { return Payload { .message = "ok" }; });

    app->Run();
}
```

`CorsMiddleware` is defined in [`src/Baldr/Middleware/Cors.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/Cors.hpp). It has no constructor dependencies and uses default options when constructed without arguments.

## How it works

For every request, the middleware writes the configured CORS headers on the response and then:

- If the request method is `OPTIONS`, the middleware sets `StatusCode::NoContent` and returns without calling `next()`. This short-circuits preflight cleanly.
- Otherwise the request is dispatched normally.

## Options

`CorsOptions`:

| Field | Default | Description |
| --- | --- | --- |
| `allowOrigin` | `"*"` | Value of `Access-Control-Allow-Origin`. Set to a concrete origin (for example `"https://app.example.com"`) when credentials are used. |
| `allowMethods` | `GET, POST, PUT, DELETE, PATCH, OPTIONS` | Values joined into `Access-Control-Allow-Methods`. |
| `allowHeaders` | `Content-Type, Authorization` | Values joined into `Access-Control-Allow-Headers`. |
| `allowCredentials` | `false` | When `true`, emits `Access-Control-Allow-Credentials: true`. The spec forbids `*` origins when credentials are enabled, so set `allowOrigin` accordingly. |
| `maxAge` | `86400` | Value of `Access-Control-Max-Age` (preflight cache duration in seconds). |

## Customising

```cpp title="src/main.cpp"
CorsMiddleware::CorsMiddleware(
    CorsOptions {
        .allowOrigin        = "https://app.example.com",
        .allowMethods       = { "GET", "POST", "OPTIONS" },
        .allowHeaders       = { "Content-Type", "Authorization", "X-Request-ID" },
        .allowCredentials   = true,
        .maxAge             = 3600,
    })
{}
```

## Where to put it

Register `CorsMiddleware` early in the pipeline so preflight responses also pass through `ExceptionHandlerMiddleware` and any logging:

```cpp title="src/main.cpp"
app->Use<RequestIdMiddleware>()
   ->Use<ExceptionHandlerMiddleware>()
   ->Use<LoggingMiddleware>()
   ->Use<CorsMiddleware>()
   ->Use<RateLimitMiddleware>();
```