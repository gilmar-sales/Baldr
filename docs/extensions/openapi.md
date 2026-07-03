# OpenAPI extension

The OpenAPI extension walks your `Router`, reads each route's [`RouteOptions`](../usage/route-options.md), and serves an **OpenAPI 3.0.3** document with JSON Schema draft-07 bodies at `/openapi.json`. It lives under `src/Baldr/extensions/OpenApi/` and is compiled directly into the `baldr` library — there is no separate target to enable.

## Enabling it

Prefer the DI-friendly extension form:

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    Baldr::OpenApi::OpenApiOptions opts;
    opts.info.title       = "My API";
    opts.info.version     = "1.0.0";
    opts.info.description = "Reference example.";

    auto builder =
        skr::ApplicationBuilder()
            .WithExtension<BaldrExtension>()
            .WithExtension<Baldr::OpenApi::BaldrOpenApiExtension>(
                [&opts](auto& ext) { ext.WithOptions(opts); });

    auto app = builder.Build<WebApplication>();
    // ...register routes...
    app->Run();
}
```

`BaldrOpenApiExtension` registers an `OpenApiSpecService` singleton in the DI container and mounts a `GET` handler at `OpenApiOptions::mountPath` once `UseServices` runs.

For an imperative flow that does not go through `skr::ApplicationBuilder`, call `Baldr::OpenApi::MapOpenApi(app, opts)` from `main()` after registering your routes:

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>
#include <Baldr/extensions/OpenApi/MapOpenApi.hpp>

int main()
{
    Baldr::WebApplication app;
    // ...register routes...
    Baldr::OpenApi::MapOpenApi(app, /* opts */ {});
    app.Run();
}
```

## Options

`OpenApiOptions` controls how the spec is generated:

| Field | Default | Description |
| --- | --- | --- |
| `mountPath` | `/openapi.json` | HTTP path that serves the rendered document. |
| `info.title` | `"Baldr API"` | `info.title` in the document. |
| `info.version` | `"0.15.1"` | `info.version` in the document. |
| `info.description` | `std::nullopt` | Optional `info.description`. |
| `enabled` | `true` | When `false`, the extension is registered but no route is mounted. |

## Served endpoint

| Path | Method | Content-Type |
| --- | --- | --- |
| `mountPath` (default `/openapi.json`) | `GET` | `application/openapi+json` |

The endpoint renders the spec lazily on the first request and caches the result in `OpenApiSpecService::mCache`. Call `OpenApiSpecService::Regenerate(router)` to rebuild on demand — useful in tests.

## Generated document shape

The renderer walks the router's `Snapshot()` and produces:

- `openapi: "3.0.3"`.
- `info` from `OpenApiOptions::info` (only `description` is emitted when present).
- `tags` — union of all route tags, alphabetised and deduplicated.
- `paths` — keyed by translated path templates (see [Path templating](#path-templating)). Each path carries one entry per HTTP method present on that template: `summary`, `description`, `operationId`, `deprecated`, `tags`, and a `responses."200"` object. A response only includes `content` when the route supplied a JSON schema.
- `components.schemas` — populated from `SchemaRegistry` when routes auto-emitted schemas via `EmitAndRegister<T>()`.

### Declaring schemas

You have three complementary ways to attach a schema to a route:

1. **Hand-written response schema** — call `.WithResponseSchemaJson("{...}")` on the registration; the string is inlined verbatim under `responses."200".content."application/json".schema`. Use this when the type is not reflectable or you need full control.

    ```cpp title="src/main.cpp"
    app->MapGet("/api/devices")
        .WithResponseSchemaJson(
            "{\"type\":\"array\",\"items\":{\"type\":\"object\","
            "\"properties\":{\"id\":{\"type\":\"integer\"},"
            "\"uuid\":{\"type\":\"string\"}}}}")
        .Handle([]() { /* ... */ });
    ```

2. **Hand-written request schema** — `.WithRequestSchemaJson("{...}")` is stashed on the route's metadata and can be consumed by custom renderers; the built-in `SpecBuilder` reads `responseSchemaJson` only.

3. **Auto-introspection** — reflectable structs whose members are limited to the [supported field types](#json-schema-dialect) are emitted by `Baldr::OpenApi::EmitStructSchema<T>()` and registered in `components.schemas/<type-name>` so they can be reused.

## JSON Schema dialect

The extension emits [JSON Schema draft-07](https://json-schema.org/draft-07/schema) using a small subset:

- `type` — `object`, `array`, `string`, `integer`, `number`, `boolean`.
- `properties` and `required` for objects.
- `$ref` references into `components.schemas/<type-name>` when the same type is used by multiple operations.

!!! note "Supported field types"
    Auto-introspection recognises `std::string`, `std::string_view`, the integral family (`int8_t`/`int16_t`/.../`uint64_t`, `int`, `long`, etc.), `float`, `double`, and `bool`. Any other member type triggers a compile-time error — fall back to `WithResponseSchemaJson(...)` with a hand-written schema.

## Path templating

| Router template | OpenAPI template |
| --- | --- |
| `/users/:id` | `/users/{id}` |
| `/files/**` | `/files/{filepath}` |

`TranslatePath` (in `RouteIntrospector.hpp`) performs the rewrite; `:name` becomes `{name}` and a trailing `**` (wildcard) becomes `{filepath}`.

## Limitations

- Swagger UI is not bundled. To mount a UI yourself, serve a static HTML file from your application and have it fetch `/openapi.json`. The [Swagger UI CDN](https://swagger.io/docs/specification/swagger-ui/) works for this.
- Deep JSON Schema features (`oneOf`, `anyOf`, `allOf`, recursive types) are out of scope for v1.
- `MapStaticFiles` routes are intentionally not included in the spec.
- Server URL is not inferred from `HttpServerOptions`; pre-render and serve a static file if you need a stable `servers` entry.
- The built-in `SpecBuilder` only reads `responseSchemaJson` from route metadata; request-body schemas, parameters, security schemes, and `requestBody` blocks are not rendered. Use the metadata hooks (`WithMetadata`, `WithRequestSchemaJson`) and a custom builder if you need them.

## Next steps

- Add metadata per route in [Route options](../usage/route-options.md).
- Browse the [`OpenApiExample`](https://github.com/gilmar-sales/Baldr/tree/main/examples/OpenApiExample) program for end-to-end usage.
- Read the source under `src/Baldr/extensions/OpenApi/` — `SpecBuilder`, `JsonSchemaEmitter`, `RouteIntrospector`, and `OpenApiSpecService` are intentionally small and easy to extend.