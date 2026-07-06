# Logging middleware

`LoggingMiddleware` writes a structured log line for every incoming request and the corresponding response. It includes the request method, path, protocol version, response status code, elapsed time, and client IP.

## Enabling it

The header is pulled in via the umbrella `<Baldr/Baldr.hpp>`. Add the middleware to your application:

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

`LoggingMiddleware` is defined in [`src/Baldr/Middleware/Logging.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/Logging.hpp). It depends on a `skr::Logger<LoggingMiddleware>`, which is provided automatically by Skirnir — no service registration is required.

## What gets logged

For a request like `GET /weather HTTP/1.1` from `192.0.2.1`, the middleware emits:

```text
Request  - 'HTTP/1.1' 'GET' '/weather'
Response - '200' 'GET' '/weather' - 312us - '192.0.2.1'
```

The first line is emitted before the request is dispatched; the second line is emitted after the response is produced and includes the elapsed microseconds.

## Where to put it

Register `LoggingMiddleware` **first**, so it wraps every other middleware and route handler:

```cpp title="src/main.cpp"
app.Use<RequestIdMiddleware>()
   .Use<ExceptionHandlerMiddleware>()
   .Use<LoggingMiddleware>()
   .Use<RateLimitMiddleware>();
```

This ensures every response — including those short-circuited by the rate limiter — is logged with accurate timing, and the request id is in scope for the log line.

## Implementation note

`LoggingMiddleware` uses C++26 reflection (`std::meta::info` via Skirnir's `refl::enum_to_string`) to format the request method. Consumers do not need to opt in to reflection themselves — the dependency is compiled inside the `baldr` target.