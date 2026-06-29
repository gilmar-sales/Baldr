# Project structure

A clean project layout makes it easy to scale a Baldr application. This page describes the conventions used by the example programs and recommended for new projects.

## Recommended layout

```
my_app/
├── CMakeLists.txt
├── README.md
├── include/
│   └── my_app/
│       └── services/
│           ├── HelloService.hpp
│           └── ...
├── src/
│   ├── main.cpp
│   └── services/
│       ├── HelloService.cpp
│       └── ...
└── tests/
    └── ...
```

This is a generalization of the [`examples/HelloService`](https://github.com/gilmar-sales/Baldr/tree/main/examples/HelloService) layout.

## Includes

Always include Baldr through the umbrella header:

```cpp
#include <Baldr/Baldr.hpp>
```

This header pulls in `BaldrExtension.hpp`, which transitively includes `WebApplication.hpp`. Individual middleware headers (such as `<Baldr/LoggingMiddleware.hpp>` and `<Baldr/RateLimitMiddleware.hpp>`) must be included explicitly when used.

## Source organization

- Put service interfaces in `include/my_app/services/`.
- Put service implementations in `src/services/`.
- Keep `main.cpp` short — only the application composition and route registration should live there. Business logic belongs in services that can be tested in isolation.

## Example: a two-file service

```cpp title="include/my_app/services/HelloService.hpp"
#pragma once

#include <Skirnir/Skirnir.hpp>

struct Payload
{
    std::string message;
};

class HelloService
{
  public:
    HelloService(const skr::Arc<skr::Logger<HelloService>> logger);

    Payload Hello(std::string name);

  private:
    skr::Arc<skr::Logger<HelloService>> mLogger;
};
```

```cpp title="src/services/HelloService.cpp"
#include "my_app/services/HelloService.hpp"

HelloService::HelloService(const skr::Arc<skr::Logger<HelloService>> logger) :
    mLogger(logger)
{
}

Payload HelloService::Hello(std::string name)
{
    mLogger->LogDebug("Hello");
    return Payload { .message = std::format("Hello, {}!", name) };
}
```

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

#include "my_app/services/HelloService.hpp"

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();

    builder.GetServiceCollection()->AddTransient<HelloService>();

    auto app = builder.Build<WebApplication>();

    app->MapGet("/hello/:name",
                [](skr::Arc<HelloService> svc, HttpRequest& req) {
                    return svc->Hello(req.params["name"]);
                });

    app->Run();
}
```

## Next steps

- Review the full list of runnable examples in [Examples](../authoring/examples.md).