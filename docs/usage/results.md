# Results

By default, returning a value from a handler makes Baldr serialize it for you — `std::string` becomes `text/plain`, JSON-serializable structs become `application/json`. When you need explicit control over status, content type, or body, return an `IResult` from the [`Result.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/Results/Result.hpp) module.

Baldr ships two flavours of result:

- **`IResult` subclasses** — free-form results that populate the response from `Apply(HttpResponse&)`. Use them when the response shape does not match a typed semantic (for example, status-only or arbitrary content types).
- **`TypedResult` subclasses** — status-specific results whose status code and (when applicable) default schema and content type are part of the *type*. Returning them — directly or inside a `std::variant` — turns into one `responses` entry per status in the generated OpenAPI document without any extra metadata.

## Quick start

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

app->MapGet("/empty",    []() -> IResult { return Results::NoContent();    });
app->MapGet("/missing",  []() -> IResult { return Results::NotFound("…");  });
app->MapGet("/logo.png", []() -> IResult {
    return ContentResult(loadPngBytes(), "image/png");
});
```

## Factory functions

`Results` is a small namespace of factory functions for the common cases. The semantic factories (`Created`, `NoContent`, `NotFound`, `BadRequest`, `Unauthorized`, `Forbidden`, `Conflict`, `UnprocessableEntity`, `InternalServerError`) return their matching `TypedResult` subclass so the OpenAPI generator emits one entry per status code; `Ok`, `Json`, and `Status` keep their back-compatible signatures.

| Factory | Returns | Status | Content-Type |
| --- | --- | --- | --- |
| `Results::Ok(body)` | `TextResult` | `200` | `text/plain` |
| `Results::Json<T, Status>(value)` | `JsonResult<T, Status>` | `Status` | `application/json` |
| `Results::Status(code)` | `StatusResult` | `code` | _empty_ |
| `Results::Created(body = "Created")` | `CreatedResult` | `201` | `text/plain` |
| `Results::NoContent()` | `NoContentResult` | `204` | _empty_ |
| `Results::BadRequest(body = "Bad Request")` | `BadRequestResult` | `400` | `text/plain` |
| `Results::Unauthorized(body = "Unauthorized")` | `UnauthorizedResult` | `401` | `text/plain` |
| `Results::Forbidden(body = "Forbidden")` | `ForbiddenResult` | `403` | `text/plain` |
| `Results::NotFound(body = "Not Found")` | `NotFoundResult` | `404` | `text/plain` |
| `Results::Conflict(body = "Conflict")` | `ConflictResult` | `409` | `text/plain` |
| `Results::UnprocessableEntity(body = "Unprocessable Entity")` | `UnprocessableEntityResult` | `422` | `text/plain` |
| `Results::InternalServerError(body = "Internal Server Error")` | `InternalServerErrorResult` | `500` | `text/plain` |

`Results::Json<T, Status>` requires `T` to be a reflectable struct (or `std::vector` of one) whose non-static data members are all in the [supported field set](#parsing-json-bodies). The body is serialised lazily through `simdjson::to_json_string` when `Apply` runs.

```cpp title="src/main.cpp"
struct CreateUserRequest { std::string username; std::string email; };
struct User             { int id; std::string username; std::string email; };

app->MapPost("/users",
    [](FromBody<CreateUserRequest> req) -> std::variant<JsonResult<User, Created>,
                                                          BadRequestResult> {
        if (req.value.username.empty())
            return Results::BadRequest("username is required");
        return JsonResult<User, Created>(createUser(req.value));
    });
```

## Built-in result types

### `IResult` and its non-typed subclasses — `<Baldr/Http/Results/Result.hpp>`

| Type | Behaviour |
| --- | --- |
| `IResult` | Abstract base. Subclasses populate an `HttpResponse` from `Apply`. |
| `TextResult(body, status = OK)` | `text/plain` with any status code. |
| `StatusResult(status)` | Status-only — empty body, no `Content-Type`. |
| `ContentResult(body, contentType, status = OK)` | Body + arbitrary `Content-Type` + status. |

### `TypedResult` subclasses — `<Baldr/Http/Results/TypedResults.hpp>`

All `TypedResult` subclasses fix the status code (and, for body-less 204, an empty `Content-Type`) in the *type*. That makes `IsTypedResultV<T>` true at compile time and lets route registration derive per-status OpenAPI metadata from the type alone — no instance required, no sample needed.

| Type | Status | Body | Content-Type |
| --- | --- | --- | --- |
| `OkResult` / `OkResult::Json<T>(value)` / `OkResult::Text(body)` | `200` | text or JSON | `text/plain` |
| `CreatedResult(body = "Created")` | `201` | text | `text/plain` |
| `NoContentResult()` | `204` | — | _empty_ |
| `BadRequestResult(body = "Bad Request")` | `400` | text | `text/plain` |
| `UnauthorizedResult(body = "Unauthorized")` | `401` | text | `text/plain` |
| `ForbiddenResult(body = "Forbidden")` | `403` | text | `text/plain` |
| `NotFoundResult(body = "Not Found")` | `404` | text | `text/plain` |
| `ConflictResult(body = "Conflict")` | `409` | text | `text/plain` |
| `UnprocessableEntityResult(body = "Unprocessable Entity")` | `422` | text | `text/plain` |
| `InternalServerErrorResult(body = "Internal Server Error")` | `500` | text | `text/plain` |
| `JsonResult<T, Status>` _(template)_ | `Status` | structured `T` | `application/json` |

!!! tip "Choose the typed factory that matches the contract"
    Returning `Results::NotFound(...)` from a handler is more than a convenience — the OpenAPI generator reads the type at registration time and emits `responses.404` with the right default schema. The same call through `Results::Status(StatusCode::NotFound)` returns a `StatusResult` instead and gets the legacy single-status treatment.

### Typed JSON results — `JsonResult<T, Status>`

`JsonResult<T, Status>` is the typed JSON return. It keeps the payload as a structured `T` and serialises through `simdjson` on `Apply`, so the OpenAPI generator can register `T` in `components.schemas` and link the per-status response with a `$ref`.

```cpp title="src/main.cpp"
struct Product { std::string id; std::string name; int price; };

app->MapGet("/products/:id",
    [](FromParams<IdParam> params) -> std::variant<JsonResult<Product, OK>,
                                                    NotFoundResult> {
        auto p = lookup(params.value.id);
        if (!p) return Results::NotFound();
        return JsonResult<Product, OK>(*p);          // 200 application/json  $ref: Product
    });
```

`JsonResult` has two construction paths:

| Form | Notes |
| --- | --- |
| `JsonResult<T, Status>(value)` | Direct constructor. Holds the structured value; serialises lazily in `Apply`. |
| `JsonResult<T, Status>::Of(value)` | Static factory, identical to the constructor. |
| `Results::Json<T, Status>(value)` | Convenience factory in the `Results` namespace. |

The template parameter `T` must satisfy `IsAutoDerivable<T>` (or be `std::vector<U>` for some auto-derivable `U`); the `static_assert` fires when the constraint is violated, surfacing the failure at registration time.

### Streaming and file results

| Type | Header | Behaviour |
| --- | --- | --- |
| `IStreamingResult` | `<Baldr/Http/Results/StreamingResult.hpp>` | Abstract base for chunked transfer-encoded bodies produced lazily. See [Streaming results](streaming-results.md). |
| `ChunkedStreamResult` | `<Baldr/Http/Results/StreamingResult.hpp>` | Streams chunks driven by a `std::function<bool(std::string&)>`. |
| `FileStreamResult(path, ...)` | `<Baldr/Http/Results/FileStreamResult.hpp>` | Streams a file with `Content-Disposition: attachment`. |

### Parser-side discriminated unions

These are not `IResult` subclasses — they describe the *parsed* side of an HTTP body or round-trip value.

| Type | Header | Behaviour |
| --- | --- | --- |
| `JsonBodyResult<T>` | `<Baldr/Http/Results/JsonBody.hpp>` | Holds either a parsed JSON value of `T` or a populated error response (`Error{statusCode, message}`). Use `parsed.isOk()` / `parsed.value()` / `parsed.takeValue()` / `parsed.error()`. |
| `HttpResult<T>` | `<Baldr/Http/Results/HttpResult.hpp>` | Plain value/error discriminated union with an explicit status code. |

## Custom results

Derive from `IResult` and implement `Apply(HttpResponse&)` to mutate the response however you need. Handlers may return any concrete `IResult` subclass by value.

```cpp title="src/main.cpp"
class HtmlResult final : public IResult
{
  public:
    explicit HtmlResult(std::string body) : mBody(std::move(body)) {}

    void Apply(HttpResponse& response) const override
    {
        response.body                   = mBody;
        response.statusCode             = StatusCode::OK;
        response.headers["Content-Type"] = "text/html; charset=utf-8";
    }

  private:
    std::string mBody;
};
```

For the OpenAPI generator to render a useful schema for a custom result, default-construct an instance and read its `StatusFor()` / `ContentTypeFor()` / `SchemaJsonFor()` accessors — the [variant walker](#variant-returns) does exactly that for legacy `IResult` alternatives.

## Variant returns

Handlers may return `std::variant<...>` to model responses that take one of several shapes — for example a product or a not-found marker. Baldr unwraps the active alternative and dispatches it through the same rules as a non-variant return: `IResult` subclasses go through `Apply`, JSON-serializable values become `application/json`, `std::string`-assignable values become `text/plain`, `std::monostate` becomes an empty `200 OK`, and so on.

```cpp title="src/main.cpp"
struct Product { std::string id; std::string name; int price; };

app->MapGet("/products/:id",
    [](FromParams<IdParam> params,
       skr::Arc<ProductRepository> repo)
       -> std::variant<JsonResult<Product, OK>, NotFoundResult> {
        auto product = repo->Find(params.value.id);
        if (!product) return Results::NotFound();
        return JsonResult<Product, OK>(*product);
    });
```

### OpenAPI: one entry per variant alternative

OpenAPI auto-derives one `responses` entry per status code from the variant alternatives:

- A `TypedResult` subclass (e.g. `NotFoundResult`, `BadRequestResult`, `JsonResult<T, Status>`) contributes its declared status with the right media type and default schema.
- A non-`TypedResult` `IResult` subclass (e.g. `TextResult`, `ContentResult`) contributes a default-constructed sample's status and content type. For `ContentResult` (whose content type is set at runtime) use `WithResponseContentType(...)` to pin the media type.
- A reflectable struct (or `std::vector` of one) contributes a `$ref` under `200` with `application/json` when no explicit response type is supplied.

User-supplied `WithResponseType<T>()` / `WithResponseSchemaJson(...)` win for status `200`; the variant-derived entry under the same status is not emitted.

!!! warning "Streaming results are not allowed inside variants"
    `IStreamingResult` alternatives inside a variant are rejected at compile time — streaming semantics assume a single owner of the response stream. Return an `IStreamingResult` directly (not wrapped in a variant) instead.

## OpenAPI content-type honesty

The OpenAPI generator records the real media type of each response instead of defaulting to `application/json`:

- `TextResult` handlers emit `text/plain`.
- `TypedResult` subclasses with `ContentTypeV` (e.g. `JsonResult<T, Status>`, `NotFoundResult`, `BadRequestResult`) emit that media type under their status.
- `ContentResult` handlers emit no media type by default (the content type is set at runtime). Pin it with `WithResponseContentType("image/png")` and pair it with `WithResponseSchemaJson(...)` or `WithResponseType<T>()`:

```cpp title="src/main.cpp"
app->MapGet("/logo.png", [](const HttpRequest&) -> IResult {
    return ContentResult(loadPngBytes(), "image/png");
})
    .WithResponseSchemaJson(R"({"type":"string","format":"binary"})")
    .WithResponseContentType("image/png");
```

`WithResponseContentType` overrides the default `application/json` for the status-`200` entry written by `WithResponseSchemaJson` / `WithResponseType`. It is ignored when no response schema is registered for status `200`.

## Parsing JSON bodies

`HttpRequest::body` is a raw `std::string`. Use the helpers in `<Baldr/Http/Results/JsonBody.hpp>` to parse it:

- `baldr::parseJsonObject(request)` returns `JsonBodyResult<simdjson::dom::object>` — either a parsed object or a populated error response.
- `baldr::parseJson<T>(request)` returns `JsonBodyResult<T>` and uses C++26 reflection to populate every non-static data member of `T` from the JSON object. Supported field types are `std::string`, `std::string_view`, the integral family (`int`/`int64_t`/`uint8_t`/...), `float`, `double`, and `bool`.

```cpp title="src/main.cpp"
struct LoginRequest { std::string username; std::string password; };

app->MapPost("/login", [](const HttpRequest& req) -> IResult {
    auto parsed = baldr::parseJson<LoginRequest>(req);
    if (!parsed.isOk())
        return Results::Status(parsed.error().statusCode);
    // ...
});
```

For the request side, `FromBody<T>` / `FromQuery<T>` / `FromParams<T>` handler arguments derive the matching OpenAPI metadata automatically — see [Route options](route-options.md).

## Next steps

- Stream large responses with [Streaming results](streaming-results.md).
- See all built-in middleware in [Middleware](../usage/middleware.md).
- Configure schema generation in [OpenAPI extension](../extensions/openapi.md).
