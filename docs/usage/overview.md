# Usage overview

A Baldr application is built around three concepts:

- An **application builder** (provided by [Skirnir](https://github.com/gilmar-sales/Skirnir)) that wires up services and extensions.
- The **Baldr extension**, which registers the router, middleware provider, and HTTP server with the builder.
- The **web application**, which exposes a strongly-typed API for registering routes and running the server.

## The minimal program

The smallest possible Baldr application looks like this:

```cpp
#include <Baldr/Baldr.hpp>

struct Payload
{
    std::string message;
};

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();

    auto app = builder.Build<WebApplication>();

    app->MapGet("/json",
                [&] { return Payload { .message = "Hello, World!" }; });

    app->Run();

    return 0;
}
```

See the [Hello World example](https://github.com/gilmar-sales/Baldr/tree/main/examples/HelloWorld) for the complete project layout.

## Application lifecycle

A Baldr program follows this lifecycle:

1. **Configure services** — register custom services on the builder's service collection.
2. **Add extensions** — call `.WithExtension<BaldrExtension>()` to wire up the router and HTTP server.
3. **Build the application** — `.Build<WebApplication>()` resolves all services and constructs the app.
4. **Register routes** — call `MapGet`, `MapPost`, or other mapping helpers.
5. **Run** — `app->Run()` starts the HTTP listener and blocks until shutdown.

## Where to go next

<div class="grid cards" markdown>

-   :material-map-marker-path: **Routing**

    Map routes, read parameters, and return responses.

    [:octicons-arrow-right-24: Routing](routing.md)

-   :material-graph-outline: **Dependency injection**

    Register and resolve services from the container.

    [:octicons-arrow-right-24: Dependency injection](dependency-injection.md)

-   :material-call-split: **Middleware**

    Intercept requests with composable middleware.

    [:octicons-arrow-right-24: Middleware](middleware.md)

</div>