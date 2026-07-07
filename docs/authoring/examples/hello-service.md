# Hello Service example

[`examples/HelloService`](https://github.com/gilmar-sales/Baldr/tree/main/examples/HelloService) introduces dependency injection: a custom `HelloService` is registered with the service collection and resolved automatically when a route handler takes it as a `skr::Arc<HelloService>` parameter. It also demonstrates returning a `std::variant` of typed results to handle the empty-path-parameter case.

## Source

[`examples/HelloService/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/HelloService/src/main.cpp):

```cpp title="examples/HelloService/src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

#include <variant>

#include "HelloService.hpp"

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    builder.GetServiceCollection()->AddTransient<HelloService>();

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet(
        "/hello/:name",
        [](skr::Arc<HelloService> helloService, baldr::HttpRequest& request)
            -> std::variant<Payload, baldr::BadRequestResult> {
            const auto& name = request.params["name"];
            if (name.empty())
                return baldr::Results::BadRequest("name is required");

            return helloService->Hello(name);
        });

    app->Run();

    return 0;
}
```

[`examples/HelloService/src/HelloService.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/HelloService/src/HelloService.hpp):

```cpp title="HelloService.hpp" linenums="1"
#pragma once

#include <Skirnir/Skirnir.hpp>

struct Payload
{
    std::string message;
};

class HelloService
{
  public:
    HelloService(const skr::Arc<skr::Logger<HelloService>> logger) :
        mLogger(logger)
    {
    }

    Payload Hello(std::string name);

  private:
    skr::Arc<skr::Logger<HelloService>> mLogger;
};
```

[`examples/HelloService/src/HelloService.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/HelloService/src/HelloService.cpp):

```cpp title="HelloService.cpp" linenums="1"
#include "HelloService.hpp"

Payload HelloService::Hello(std::string name)
{
    mLogger->LogDebug("Hello");
    return Payload { .message = std::format("Hello, {}!", name) };
}
```

## What it shows

- Registering a service via `AddTransient<HelloService>()`.
- Logger injection via `skr::Logger<HelloService>` — Skirnir provides loggers automatically; no explicit registration.
- Reading path parameters from `HttpRequest::params`.
- Returning a `std::variant` of typed results (`Payload` on success, `baldr::BadRequestResult` on failure) instead of throwing or hand-building error responses.
- Returning a custom type that the framework serialises to JSON.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/HelloService
```

In another terminal:

```bash
curl http://localhost:8080/hello/world
# {"message":"Hello, world!"}
```

## Next steps

- See [Dependency injection](../../usage/dependency-injection.md) for the full DI reference.
- See [Results](../../usage/results.md) for the typed result family returned from the `std::variant`.
- Browse [all examples](../examples.md).
