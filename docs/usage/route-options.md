# Route options

Every Baldr route can carry per-route metadata — summary, description, tags, an operation id, a deprecation flag, and arbitrary string key/value pairs. Middleware and the [OpenAPI extension](../extensions/openapi.md) read this metadata to apply policy and to render documentation.

## Fluent builder

`WebApplication::MapGet`, `MapPost`, `MapPut`, `MapDelete`, and `MapPatch` each return a fluent `RouteRegistration` when called with a single path argument. Chain option setters on the result, then call `.Handle(handler)` to bind the route.

```cpp title="src/main.cpp"
app->MapGet("/api/devices")
    .WithSummary("List devices")
    .WithOperationId("listDevices")
    .WithTag("devices")
    .Handle([]() { return std::vector<Device> { /* ... */ }; });
```

The legacy two-argument form `app->MapGet(path, handler)` still compiles and produces a route with default options.

## Available options

| Method | Effect |
|---|---|
| `WithSummary(std::string)` | Short, single-sentence description. |
| `WithDescription(std::string)` | Long-form description. |
| `WithTag(std::string)` | Adds a tag (may be called multiple times). |
| `WithTags(std::vector<std::string>)` | Replaces the tag list. |
| `WithOperationId(std::string)` | Unique operation identifier. |
| `WithDeprecated(bool = true)` | Marks the operation as deprecated. |
| `WithConsumes(std::vector<std::string>)` | Accepted request content types. |
| `WithProduces(std::vector<std::string>)` | Produced response content types. |
| `WithMetadata(std::string, std::string)` | Free-form key/value entry. |
| `WithRequestSchemaJson(std::string)` | Raw JSON Schema for the request body. |
| `WithResponseSchemaJson(std::string)` | Raw JSON Schema for the response body. |

## Reading options in middleware

`HttpRequest` carries a `RouteInfo` member populated from the matched `RouteEntry` before middleware runs:

```cpp title="src/middleware.cpp"
class AuditMiddleware : public IMiddleware
{
  public:
    void Handle(HttpRequest& req, HttpResponse& /*res*/,
                std::function<void()> next) override
    {
        if (req.route.options.deprecated)
        {
            // log a deprecation warning for the matched route
        }
        next();
    }
};
```

`request.route.path` is the resolved template (e.g. `/api/v1/users/:id`), `request.route.method` is the HTTP method, and `request.route.group` is the prefix from the enclosing `MapGroup`, if any.

## Route groups

`MapGroup(prefix, setup)` registers a group of routes that share a common URL prefix. `setup` receives a `RouteBuilder` exposing the same `MapGet`/`MapPost`/... fluent API:

```cpp title="src/main.cpp"
app->MapGroup("/api/v1", [](auto& group) {
    group.MapGet("/users")
        .WithSummary("List users")
        .WithTag("users")
        .Handle([]() { return std::vector<User>{}; });

    group.MapPost("/users", [](const HttpRequest& req) -> IResult {
        // ...
        return Results::Status(StatusCode::Created);
    });
});
```

The prefix is concatenated with each route's template when matching. Route options (summary, tags, operation id, schemas) are still applied per-route inside the group.

## Static files

Use `MapStaticFiles(urlPrefix, rootPath)` to serve a directory tree under a URL prefix — see [Static files](static-files.md) for path safety, MIME-type inference, and streaming behaviour.

```cpp title="src/main.cpp"
app->MapStaticFiles("/static", "/var/www/my_app/wwwroot");
```

## Next steps

- Generate an OpenAPI 3.0.3 spec from your routes in the [OpenAPI extension](../extensions/openapi.md).
- Compose cross-cutting concerns in [Middleware](middleware.md).