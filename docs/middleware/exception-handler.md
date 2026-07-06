# Exception handler middleware

`ExceptionHandlerMiddleware` converts thrown exceptions into a structured 500 response. It is the recommended last line of defence in any production pipeline.

## Enabling it

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app.Use<RequestIdMiddleware>()
       .Use<ExceptionHandlerMiddleware>();

    app->MapGet("/boom", [](HttpRequest&) -> IResult {
        throw std::runtime_error("kaboom");
        return Results::Ok("unreachable");
    });

    app->Run();
}
```

`ExceptionHandlerMiddleware` is defined in [`src/Baldr/Middleware/ExceptionHandler.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/ExceptionHandler.hpp). It has no constructor dependencies.

## How it works

The middleware wraps the call to `next()` in a `try`/`catch`:

- Catches `const std::exception&` and invokes the configured mapper to produce a response body.
- Catches everything else (`catch (...)`) and produces a fixed body — the mapper is **not** called for non-`std::exception` throws because no exception object is available.

In both cases the response status is set to `ExceptionHandlerOptions::status` (default `StatusCode::InternalServerError`) and the configured `contentType` is written.

## Options

`ExceptionHandlerOptions`:

| Field | Default | Description |
| --- | --- | --- |
| `mapper` | built-in | `std::function<std::string(const std::exception&)>` invoked when a typed exception is caught. The built-in mapper returns `"Internal Server Error"` unless `includeDetailsInDev` is `true`. |
| `includeDetailsInDev` | `false` | When `true`, the built-in mapper returns `e.what()` instead of the generic message. Keep this `false` in production to avoid leaking internal exception text to clients. |
| `contentType` | `"text/plain"` | Content-Type written on the produced response. |
| `status` | `StatusCode::InternalServerError` | Status code emitted on any caught exception. |

Custom mapper example:

```cpp title="src/main.cpp"
ExceptionHandlerMiddleware::ExceptionHandlerMiddleware(
    ExceptionHandlerOptions {
        .mapper =
            [](const std::exception& e) {
                spdlog::error("unhandled: {}", e.what());
                return std::string("Internal Server Error");
            },
        .status = StatusCode::InternalServerError,
    })
{}
```

!!! warning "Don't log the body in production"
    Even when you log the exception details server-side, the body sent to the client should still be a generic message unless you know the exception text is safe to expose.

## Where to put it

Register `ExceptionHandlerMiddleware` **immediately after** `RequestIdMiddleware` so the request id is available to whatever logging the mapper triggers:

```cpp title="src/main.cpp"
app.Use<RequestIdMiddleware>()
   .Use<ExceptionHandlerMiddleware>()
   .Use<LoggingMiddleware>()
   .Use<RateLimitMiddleware>();
```