# Hello World example

[`examples/HelloWorld`](https://github.com/gilmar-sales/Baldr/tree/main/examples/HelloWorld) is the smallest possible Baldr program — one route that returns a JSON payload.

## Source

[`examples/HelloWorld/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/HelloWorld/src/main.cpp):

```cpp title="examples/HelloWorld/src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

#include <variant>

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
                [&]() { return Payload { .message = "Hello, World!" }; });

    app->Run();

    return 0;
}
```

## What it shows

- Application composition with `skr::ApplicationBuilder` and `baldr::BaldrExtension`.
- Building the host with `builder.Build<baldr::WebApplication>()`.
- Route registration with `MapGet(path, handler)`.
- Automatic JSON serialization of a returned aggregate — `Payload` is serialised to `{"message":"Hello, World!"}` with `Content-Type: application/json`.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/HelloWorld
```

In another terminal:

```bash
curl http://localhost:8080/json
```

## Next steps

- See [Hello Service](hello-service.md) for the DI version of this program.
- Browse [all examples](../examples.md).
