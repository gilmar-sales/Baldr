# Request-ID middleware

`RequestIdMiddleware` ensures every request carries an `X-Request-ID` header. If the inbound request already has one, it is echoed; otherwise the middleware generates a fresh id. The same value is written to the response so clients and proxies can correlate logs.

## Enabling it

The header is pulled in via `<Baldr/Baldr.hpp>`. Add the middleware to your application:

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->Use<RequestIdMiddleware>();
    app->Use<LoggingMiddleware>();

    app->MapGet("/", [] { return Payload { .message = "ok" }; });

    app->Run();
}
```

`RequestIdMiddleware` is defined in [`src/Baldr/Middleware/RequestId.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/RequestId.hpp). It has no constructor dependencies and no options — register it via `Use<RequestIdMiddleware>()`.

## Behaviour

The middleware reads the inbound `X-Request-ID` header (case-insensitive). When present and non-empty it is reused; otherwise a fresh id is generated. The id is then:

- Stored on `request.headers["X-Request-ID"]` so downstream handlers and middleware can read it.
- Written on `response.headers["X-Request-ID"]` so the client sees the same value.

## Where to put it

Register `RequestIdMiddleware` **first**, so every other middleware and log line can read the id:

```cpp title="src/main.cpp"
app.Use<RequestIdMiddleware>()
   .Use<ExceptionHandlerMiddleware>()
   .Use<LoggingMiddleware>();
```

## Notes

The generated id is **not cryptographically secure** — it is a 128-bit mix of the wall clock and a thread-local counter. It is suitable for log correlation within a single deployment but should not be relied on as a security token. For CSRF protection, see [CSRF middleware](csrf.md), which uses a dedicated double-submit cookie.