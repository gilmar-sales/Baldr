# 2. Routing and results

Baldr ships one registration method per HTTP verb: `MapGet`, `MapPost`, `MapPut`, `MapDelete`, `MapPatch`. Each accepts a path template and a handler, and each returns a fluent `RouteRegistration` for chaining options.

## Multiple methods, same path

```cpp title="src/main.cpp" linenums="1"
app->MapGet("/widgets",
            []() { return std::vector<Widget>{}; });

app->MapPost("/widgets",
             [](const HttpRequest& req) -> IResult {
                 Widget w { /* parse req.body */ };
                 // ...persist w...
                 return Results::Status(StatusCode::Created);
             });
```

When a path exists for one method and another method is requested, the framework returns `405 Method Not Allowed` with an `Allow` header listing the valid methods for that path.

## Path templates

Templates accept `:name` parameters and a trailing `**` catch-all:

```cpp title="src/main.cpp"
app->MapGet("/users/:id", [](const HttpRequest& req) {
    auto id = req.params.at("id");
    // ...
});

app->MapGet("/files/**", [](const HttpRequest& req) {
    auto path = req.params.at("filepath");
    // ...
});
```

Path parameters land in `request.params`. See [Route groups](../usage/route-options.md#route-groups) for grouping under a common prefix.

## Returning data

Returning a reflectable aggregate serialises to JSON:

```cpp title="src/main.cpp"
app->MapGet("/me", []() {
    return Profile { .name = "Ada", .score = 42 };
});
// {"name":"Ada","score":42}
```

Returning a string sends `text/plain`:

```cpp title="src/main.cpp"
app->MapGet("/ping", []() { return std::string("pong"); });
```

For full control, return an `IResult`:

```cpp title="src/main.cpp"
app->MapGet("/legacy", [](const HttpRequest&) -> IResult {
    return Results::Status(StatusCode::Gone);
});
```

See [Results](../usage/results.md) for the complete family of `Results::*` factories and typed results.

## Next

Continue with [3. Dependency injection](03-dependency-injection.md).