# Results

By default, returning a value from a handler makes Baldr serialize it for you — `std::string` becomes `text/plain`, JSON-serializable structs become `application/json`. When you need explicit control over status, content type, or body, return an `IResult` from the [`Result.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Result.hpp) module.

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
    // Serve raw bytes with a specific content type.
    return ContentResult(imageBytes, "image/png");
});
```

## Factory functions

- `Results::Ok(body)` — 200 with `text/plain`.
- `Results::Json(body)` — 200 with `application/json`.
- `Results::Status(code)` — empty body, sets the status.
- `Results::NotFound(body = "Not Found")` — 404 with `text/plain`.

`ContentResult(body, contentType, status = OK)` accepts an arbitrary body and content type, useful for static files, images, or other non-JSON payloads.

## Custom results

Derive from `IResult` and implement `Apply(HttpResponse&)` to mutate the response however you need. Handlers may return any concrete `IResult` subclass by value.