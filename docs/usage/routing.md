# Routing

Routing is how Baldr maps an incoming HTTP request to a C++ handler. Handlers can read the request, return any serializable value, and have dependencies injected automatically.

## Registering routes

Routes are registered on a `WebApplication` instance via `MapGet` and `MapPost`:

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

struct Payload { std::string message; };

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->MapGet("/hello", [] { return Payload { .message = "Hello!" }; });
    app->MapPost("/echo", [](HttpRequest& req) {
        return Payload { .message = req.body };
    });

    app->Run();
}
```

`WebApplication::MapGet` and `MapPost` are defined in [`src/Baldr/WebApplication.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/WebApplication.hpp) as templated wrappers around the underlying router.

## Path parameters

Declare path parameters by prefixing a segment with `:`. The parameter is available from the request:

```cpp title="src/main.cpp"
app->MapGet("/users/:id", [](HttpRequest& request) {
    return Payload { .message = "user " + request.params["id"] };
});
```

This is demonstrated in the [`examples/HelloService`](https://github.com/gilmar-sales/Baldr/tree/main/examples/HelloService) program, where the route `/hello/:name` reads `request.params["name"]`.

## Return values and JSON serialization

Handlers can return:

- `void` — useful for handlers that write to the `HttpResponse` directly.
- A `std::string` — returned as `text/plain`.
- Any aggregate or container of fields — automatically serialized to JSON via [`simdjson`](https://github.com/simdjson/simdjson) and returned as `application/json` with HTTP `200 OK`.

```cpp title="src/main.cpp"
app->MapGet("/devices", []() {
    return std::vector<Device> {
        Device { .id = 1, .name = "Sensor A" },
        Device { .id = 2, .name = "Sensor B" },
    };
});
```

See [`examples/Devices/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/Devices/src/main.cpp) for a full list-based handler.

## Reading the request

Handlers may declare an `HttpRequest&` parameter to access headers, the body, the path, query parameters, and the client IP:

```cpp title="src/main.cpp"
app->MapPost("/echo", [](HttpRequest& request) {
    auto it = request.headers.find("Content-Type");
    std::string contentType = it != request.headers.end() ? it->second : "";
    return Payload { .message = request.body };
});
```

## Next steps

- Learn how handlers receive injected services in [Dependency injection](dependency-injection.md).
- Compose handlers with cross-cutting concerns in [Middleware](middleware.md).