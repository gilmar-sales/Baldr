# Dependency injection

Baldr is built on [Skirnir](https://github.com/gilmar-sales/Skirnir)'s dependency injection container. Services are registered on the builder before `.Build<WebApplication>()` is called and can then be injected into route handlers.

## Registering services

Use the builder's service collection to register your own services:

```cpp title="src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>
#include "HelloService.hpp"

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();

    builder.GetServiceCollection()->AddTransient<HelloService>();

    auto app = builder.Build<WebApplication>();

    app->MapGet("/hello/:name",
                [](skr::Arc<HelloService> helloService, HttpRequest& request) {
                    return helloService->Hello(request.params["name"]);
                });

    app->Run();
}
```

This example is the [`examples/HelloService`](https://github.com/gilmar-sales/Baldr/tree/main/examples/HelloService) program. `HelloService` is registered as transient and resolved automatically by the framework.

## How injection works

When you register a route handler, Baldr inspects the handler's parameters:

- A `HttpRequest&` parameter is satisfied with the current request.
- A `HttpResponse&` parameter is satisfied with the current response.
- A `skr::Arc<T>` parameter is resolved from the service provider — T must be registered in the service collection.

This inspection is implemented in [`src/Baldr/WebApplication.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/WebApplication.hpp) using the `LambdaTraits` and `transformTuple` helpers from [`src/Baldr/Tuple.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Tuple.hpp).

## Logger injection

Any service that depends on a `skr::Logger<T>` receives a logger automatically — no explicit registration is required:

```cpp title="HelloService.hpp"
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

The injected logger is used in the implementation:

```cpp title="HelloService.cpp"
#include "HelloService.hpp"

Payload HelloService::Hello(std::string name)
{
    mLogger->LogDebug("Hello");
    return Payload { .message = std::format("Hello, {}!", name) };
}
```

## Lifetime scopes

Skirnir's standard lifetimes (`AddTransient`, `AddScoped`, `AddSingleton`) work as expected. Pick the lifetime that matches your service:

- **Transient** — a new instance per resolution. Good for stateless services.
- **Scoped** — one instance per request scope.
- **Singleton** — one instance for the lifetime of the application.

Refer to the Skirnir documentation for the full DI semantics.