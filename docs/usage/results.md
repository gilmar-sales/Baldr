# Results

By default, returning a value from a handler makes Baldr serialize it for you — `std::string` becomes `text/plain`, JSON-serializable structs become `application/json`. When you need explicit control over status, content type, or body, return an `IResult` from the [`Result.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/Results/Result.hpp) module.

## Built-in results

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

app->MapGet("/empty", []() -> IResult {
    return Results::Status(StatusCode::NoContent);
});

app->MapGet("/missing", []() -> IResult {
    return Results::NotFound("Nothing here.");
});

app->MapGet("/image", []() -> IResult {
    return ContentResult(imageBytes, "image/png");
});
```

## Factory functions

`Results` is a small namespace of factory functions for the common cases:

- `Results::Ok(body)` — `200 OK` with `text/plain`.
- `Results::Json(body)` — `200 OK` with `application/json`. Calls `simdjson::to_json_string` on the value.
- `Results::Status(code)` — empty body, sets the status.
- `Results::NotFound(body = "Not Found")` — `404` with `text/plain`.

`ContentResult(body, contentType, status = OK)` accepts an arbitrary body and content type — useful for static files, images, or other non-JSON payloads.

## Built-in result types

| Type | Header | Behaviour |
| --- | --- | --- |
| `TextResult` | `<Baldr/Http/Results/Result.hpp>` | Body + status, sets `Content-Type: text/plain`. |
| `JsonResult` | `<Baldr/Http/Results/Result.hpp>` | Body + status, sets `Content-Type: application/json`. |
| `StatusResult` | `<Baldr/Http/Results/Result.hpp>` | Status only, no body. |
| `ContentResult` | `<Baldr/Http/Results/Result.hpp>` | Body + custom content type + status. |
| `IStreamingResult` | `<Baldr/Http/Results/StreamingResult.hpp>` | Chunked transfer-encoded body produced lazily. See [Streaming results](streaming-results.md). |
| `ChunkedStreamResult` | `<Baldr/Http/Results/StreamingResult.hpp>` | Streaming driven by a user-supplied callback. |
| `FileStreamResult` | `<Baldr/Http/Results/FileStreamResult.hpp>` | Streams a file with `Content-Disposition: attachment`. |
| `JsonResult<T>` | `<Baldr/Http/Results/JsonBody.hpp>` | Holds either a parsed JSON value or an error response. |
| `HttpResult<T>` | `<Baldr/Http/Results/HttpResult.hpp>` | Plain value/error discriminated union with explicit status. |

## Custom results

Derive from `IResult` and implement `Apply(HttpResponse&)` to mutate the response however you need. Handlers may return any concrete `IResult` subclass by value.

```cpp title="src/main.cpp"
class HtmlResult final : public IResult
{
  public:
    explicit HtmlResult(std::string body) : mBody(std::move(body)) {}

    void Apply(HttpResponse& response) const override
    {
        response.body             = mBody;
        response.statusCode       = StatusCode::OK;
        response.headers["Content-Type"] = "text/html; charset=utf-8";
    }

  private:
    std::string mBody;
};
```

## Parsing JSON bodies

`HttpRequest::body` is a raw `std::string`. Use the helpers in `<Baldr/Http/Results/JsonBody.hpp>` to parse it:

- `Baldr::parseJsonObject(request)` returns `JsonResult<simdjson::dom::object>` — either a parsed object or a populated error response.
- `Baldr::parseJson<T>(request)` returns `JsonResult<T>` and uses C++26 reflection to populate every non-static data member of `T` from the JSON object. Supported field types are `std::string`, `std::string_view`, `int`, `int64_t`, `double`, and `bool`.

```cpp title="src/main.cpp"
struct LoginRequest
{
    std::string username;
    std::string password;
};

app->MapPost("/login", [](const HttpRequest& req) -> IResult {
    auto parsed = Baldr::parseJson<LoginRequest>(req);
    if (!parsed.isOk())
        return Results::Status(parsed.error().statusCode);
    // ...
});
```

## Next steps

- Stream large responses with [Streaming results](streaming-results.md).
- See all built-in middleware in [Middleware](../usage/middleware.md).