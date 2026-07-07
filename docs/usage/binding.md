# Request binding

`FromBody<T>`, `FromQuery<T>`, and `FromParams<T>` are typed alternatives to
reading `HttpRequest::body`, `HttpRequest::query`, and `HttpRequest::params`
by hand. Declare a parameter of one of these wrapper types in a handler and
the router resolves it before the handler runs, handing back a small value
shell that holds either the parsed payload or an error describing why the
bind failed.

The three wrappers live in:

- [`<Baldr/Http/FromBody.hpp>`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/FromBody.hpp)
- [`<Baldr/Http/FromQuery.hpp>`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/FromQuery.hpp)
- [`<Baldr/Http/FromParams.hpp>`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/FromParams.hpp)

## Common shape

All three wrappers expose the same surface:

| Member | Type | Meaning |
| --- | --- | --- |
| `value` | `T` | Parsed payload, default-constructed on bind failure. Public member. |
| `error` | `std::optional<…>` | Populated only on failure; `std::nullopt` on success. |
| `isOk()` | `bool` | `!error.has_value()`. |
| `Error()` / `getError()` | `std::optional<…>` | Returns a copy of the bind error. Named `Error()` on `FromBody` and `getError()` on `FromQuery` / `FromParams` to mirror the [`JsonBodyResult`](results.md#parser-side-discriminated-unions) convention. |

`value` is intentionally a public data member — there is no `Value()` accessor
wrapping it. Read it directly as `req.value.field` once `isOk()` returns
`true`.

When the bind fails, the router short-circuits the handler and writes a
structured error response itself (typically `400 Bad Request` with a JSON or
plain-text body, or `415 Unsupported Media Type` for a non-JSON `FromBody`).
Handlers therefore only run when the payload is parseable; they may still
inspect `error` to react to a partial parse.

## `FromBody<T>`

`FromBody<T>` parses the request body as JSON into a reflectable struct `T`
through [`parseJson`](results.md#parsing-json-bodies). The router runs
`bindFromBody` before the handler; on success the handler is invoked with
`isOk() == true` and `value` holding the parsed `T`.

The bind fails (and the handler is not called) when:

- The `Content-Type` header is present and is not
  `application/json` (case-insensitive). The bind reports
  `415 Unsupported Media Type`.
- The body is missing a required field, has a wrong type, or is not valid
  JSON. The bind reports `400 Bad Request` with a per-field error message
  via [`JsonBodyResult`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/Results/JsonBody.hpp).

```cpp title="src/main.cpp" linenums="1"
struct CreateUserRequest
{
    std::string username;
    std::string email;
    int         age {};
};

app->MapPost("/users",
    [](baldr::FromBody<CreateUserRequest> req)
        -> std::variant<JsonResult<User, Created>, BadRequestResult> {
        // req.isOk() is guaranteed true here; the router short-circuits on failure.
        if (req.value.username.empty())
            return Results::BadRequest("username is required");
        if (req.value.age < 0)
            return Results::BadRequest("age must be non-negative");
        return JsonResult<User, Created>(createUser(req.value));
    });
```

`T` must satisfy the same constraints as every other JSON-deserialised type
in Baldr: every non-static data member must be `std::string`,
`std::string_view`, an integral type, `double`, `float`, or `bool`; or a
specialisation of `baldr::detail::readJsonField` must exist for it. See
[Parsing JSON bodies](results.md#parsing-json-bodies) for the full list.

## `FromQuery<T>`

`FromQuery<T>` aggregates the parsed query string (`HttpRequest::query`) into
a reflectable struct. Each non-static data member of `T` is looked up by
name in the query map; the bind fails with `400 Bad Request` if any member
is missing or cannot be parsed as the member's type.

```cpp title="src/main.cpp" linenums="1"
struct SearchFilters
{
    std::string q;
    int         minAge {};
    bool        activeOnly {};
};

app->MapGet("/search",
    [](baldr::FromQuery<SearchFilters> f) -> IResult {
        // f.isOk() is guaranteed true here.
        return Results::Json<std::vector<User>, OK>(search(f.value));
    });
```

Booleans accept `"true"`, `"false"`, `"1"`, and `"0"`. Numeric members use
`std::stod` / `std::stoll` on the raw value and reject partial parses
(e.g. `"42abc"` fails for `int`).

## `FromParams<T>`

`FromParams<T>` aggregates path parameters (the segments declared with
`:name` in the route template) into a reflectable struct. The bind is
otherwise identical to `FromQuery<T>` — every member of `T` must have a
matching `:name` segment and parse as the member's type, otherwise the
bind fails with `400 Bad Request`.

```cpp title="src/main.cpp" linenums="1"
struct IdParam
{
    std::string id;
};

struct Product
{
    std::string id;
    std::string name;
    int         price {};
};

app->MapGet("/products/:id",
    [](baldr::FromParams<IdParam> params)
        -> std::variant<JsonResult<Product, OK>, NotFoundResult> {
        auto p = lookup(params.value.id);
        if (!p) return Results::NotFound();
        return JsonResult<Product, OK>(*p);
    });
```

## Combining wrappers

A handler may declare any combination of `FromBody`, `FromQuery`, and
`FromParams` parameters, in any order, alongside injected services. The
router resolves each wrapper independently and short-circuits the whole
handler if any of them fails to bind.

```cpp title="src/main.cpp" linenums="1"
struct UpdateDeviceDto
{
    std::string name;
    std::string firmware;
};

app->MapPut("/devices/:id",
    [](baldr::FromParams<IdParam> params,
       baldr::FromBody<UpdateDeviceDto> body,
       skr::Arc<DeviceRepository> repo)
       -> std::variant<JsonResult<Device, OK>, NotFoundResult> {
        auto device = repo->Find(params.value.id);
        if (!device) return Results::NotFound();
        device->name     = body.value.name;
        device->firmware = body.value.firmware;
        repo->Save(*device);
        return JsonResult<Device, OK>(*device);
    });
```

The same code path is implemented by
[`RouteRegistration::Handle`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/RouteRegistration.hpp)
— see [Route options](route-options.md) for the surrounding route metadata.

## OpenAPI integration

The framework reflects on the handler's signature at registration time and
derives an OpenAPI `requestBody` / `parameters` block from any
`FromBody<T>`, `FromQuery<T>`, or `FromParams<T>` parameter it finds. This is
covered in detail in [OpenAPI metadata](../extensions/openapi.md); the short
version is: declaring the wrapper in the signature is enough — no extra
configuration is required for the generated spec to expose the request
schema.

Use the explicit `WithRequestType<T>()` / `WithQueryType<T>()` /
`WithPathType<T>()` builders only when the handler argument is a bare
`HttpRequest&` or you need to override the inferred schema.

## Where to go next

<div class="grid cards" markdown>

-   :material-map-marker-path: **Routing**

    Map routes, read parameters, and return responses.

    [:octicons-arrow-right-24: Routing](routing.md)

-   :material-arrow-right-bold-box-outline: **Results**

    Typed return values, status codes, and `JsonResult` for the response side.

    [:octicons-arrow-right-24: Results](results.md)

-   :material-book-open-variant: **OpenAPI**

    Auto-derived request and response schemas from the handler signature.

    [:octicons-arrow-right-24: OpenAPI](../extensions/openapi.md)

</div>
