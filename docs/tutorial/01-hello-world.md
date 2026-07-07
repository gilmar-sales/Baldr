# 1. Hello world

A complete Baldr application in a single file.

## Source

```cpp title="src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

struct Payload
{
    std::string message;
};

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet("/json",
                []() { return Payload { .message = "Hello, World!" }; });

    app->Run();

    return 0;
}
```

## Build and run

```bash title="terminal"
cmake -S . -B build
cmake --build build --config Release
./build/my_app
```

In another terminal:

```bash title="terminal"
curl http://localhost:8080/json
```

Response:

```json title="Response"
{ "message": "Hello, World!" }
```

## What just happened?

The four lines that turn this into an HTTP server are:

1. `skr::ApplicationBuilder()` — Skirnir's generic host builder.
2. `.WithExtension<baldr::BaldrExtension>()` — registers Baldr's services (router, middleware provider, HTTP server).
3. `.Build<baldr::WebApplication>()` — constructs the strongly-typed application.
4. `app->MapGet("/json", handler)` — registers a handler for `GET /json`.

The handler returns a `Payload` aggregate, and the framework serialises it to JSON automatically with `Content-Type: application/json`.

## Next

Continue with [2. Routing and results](02-routing-and-results.md).