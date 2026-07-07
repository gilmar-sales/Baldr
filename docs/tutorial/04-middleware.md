# 4. Middleware

Middleware runs before and after every request, in registration order. Baldr ships a few useful middleware and a simple interface for adding your own.

## Built-in middleware

```cpp title="src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    auto app = builder.Build<baldr::WebApplication>();

    app->Use<RequestIdMiddleware>()
        .Use<LoggingMiddleware>();

    app->MapGet("/", []() { return std::string("hi"); });

    app->Run();
    return 0;
}
```

The two middleware above:

- `RequestIdMiddleware` — attaches an `X-Request-Id` header to every response. If the request already has one, it is reused; otherwise a fresh id is generated.
- `LoggingMiddleware` — emits a one-line access log entry per request.

## Custom middleware

Implement `IMiddleware::Handle`:

```cpp title="src/middleware.hpp" linenums="1"
#pragma once
#include <Baldr/Middleware/IMiddleware.hpp>

class TimingMiddleware : public baldr::IMiddleware
{
  public:
    void Handle(baldr::HttpRequest&          req,
                baldr::HttpResponse&         res,
                const baldr::NextMiddleware& next) override
    {
        auto start = std::chrono::steady_clock::now();
        next();
        auto elapsed = std::chrono::steady_clock::now() - start;
        res.headers["X-Elapsed-Us"] =
            std::to_string(std::chrono::duration_cast<
                std::chrono::microseconds>(elapsed).count());
    }
};
```

Register it the same way as the built-ins:

```cpp title="src/main.cpp"
app->Use<TimingMiddleware>();
```

## Reading route metadata

Inside a middleware, `req.route.options` carries the metadata attached via `RouteRegistration::WithMetadata` — see [Route options](../usage/route-options.md) for the full list.

## Next

Continue with [5. Static files](05-static-files.md).