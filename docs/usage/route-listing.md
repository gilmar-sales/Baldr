# Route listing (debug)

`WebApplication::EnableRouteListing` registers an HTTP endpoint that returns a JSON snapshot of every route currently registered with the router. It is intended for development and debugging.

!!! warning "Debug only"
    The endpoint is compiled out of release builds via the standard @c NDEBUG macro. Calling @c EnableRouteListing in a release build is a no-op — no endpoint is registered. Do not rely on the endpoint in production.

## Basic usage

```cpp title="src/main.cpp"
auto app = skr::ApplicationBuilder()
               .WithExtension<BaldrExtension>()
               .Build<WebApplication>();

app->MapGet("/users",  []() { return std::vector<User>{}; });
app->MapPost("/users", []() { return User{}; });

app->EnableRouteListing();   // serves on /__routes by default
app->Run();
```

`curl http://localhost:8080/__routes` returns:

```json title="Response"
{
  "routes": [
    { "method": "GET",  "path": "/users",  "group": "", "metadata": {} },
    { "method": "POST", "path": "/users",  "group": "", "metadata": {} }
  ]
}
```

## Custom path

```cpp title="src/main.cpp"
app->EnableRouteListing("/_debug/routes");
```

## What is exposed

Each entry reports:

| Field | Source |
|---|---|
| `method` | The HTTP method (`GET`, `POST`, …). |
| `path`   | The resolved path template (after `MapGroup` prefix concatenation). |
| `group`  | The `MapGroup` prefix, or `""` if the route is not grouped. |
| `metadata` | The free-form key/value bag populated via `WithMetadata` and the OpenAPI schema stashes (`requestSchemaJson`, `responseSchemaJson`, …). |

## Use cases

- **Inspecting route registration** while developing without a debugger.
- **Auditing route exposure** — list every URL the application exposes at a glance.
- **Building introspection tools** — pipe the JSON into `jq` or another script.

## Next steps

- Render an OpenAPI spec from the same router snapshot in the [OpenAPI extension](../extensions/openapi.md).
- Combine with [Middleware](middleware.md) to redact sensitive metadata before exposure.