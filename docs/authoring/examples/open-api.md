# OpenAPI example

[`examples/OpenApiExample`](https://github.com/gilmar-sales/Baldr/tree/main/examples/OpenApiExample) shows the [`BaldrOpenApiExtension`](../../extensions/openapi.md) end-to-end: route metadata (`WithSummary`, `WithOperationId`, `WithTag`) feeds an auto-generated OpenAPI 3.0.3 document, and the Scalar UI is mounted alongside it.

## Source

[`examples/OpenApiExample/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/OpenApiExample/src/main.cpp):

```cpp title="examples/OpenApiExample/src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

#include <optional>
#include <variant>

#include "Device.hpp"
#include "User.hpp"

namespace
{
    std::vector<User> makeUsers()
    {
        return std::vector<User> { User { .id = 1, .name = "First" },
                                   User { .id = 2, .name = "Second" } };
    }

    std::optional<User> findUser(int id)
    {
        for (const auto& u : makeUsers())
        {
            if (u.id == id)
                return u;
        }
        return std::nullopt;
    }
} // namespace

int main()
{
    auto builder =
        skr::ApplicationBuilder()
            .WithExtension<baldr::BaldrExtension>()
            .WithExtension<baldr::BaldrOpenApiExtension>(
                [](baldr::BaldrOpenApiExtension& openApi) {
                    baldr::OpenApiOptions opts;
                    opts.info.title       = "Devices API";
                    opts.info.version     = "1.0.0";
                    opts.info.description = "Reference example demonstrating "
                                            "RouteOptions + OpenAPI 3.0.3.";
                    openApi.WithOptions(opts);
                });

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGroup("/api/v1", [](auto& group) {
        group.MapGet("/users")
            .WithSummary("Fetch users")
            .WithTag("users")
            .Handle([](baldr::HttpRequest&) { return makeUsers(); });

        group.MapGet("/users/:id")
            .WithSummary("Get a user by id")
            .WithTag("users")
            .Handle([](baldr::HttpRequest& request)
                        -> std::variant<
                            baldr::JsonResult<User, baldr::StatusCode::OK>,
                            baldr::BadRequestResult, baldr::NotFoundResult> {
                int id = 0;
                try
                {
                    id = std::stoi(request.params.at("id"));
                }
                catch (...)
                {
                    return baldr::Results::BadRequest();
                }

                auto found = findUser(id);
                if (!found)
                    return baldr::Results::NotFound();

                return baldr::Results::Json<User, baldr::StatusCode::OK>(
                    *found);
            });
    });

    baldr::MapScalarUi(*app);

    app->Run();

    return 0;
}
```

[`examples/OpenApiExample/src/Device.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/OpenApiExample/src/Device.hpp):

```cpp title="Device.hpp"
#pragma once

#include <string>

struct Device
{
    int         id;
    std::string uuid;
    std::string mac;
    std::string firmware;
    std::string created_at;
    std::string updated_at;
};
```

[`examples/OpenApiExample/src/User.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/OpenApiExample/src/User.hpp):

```cpp title="User.hpp"
#pragma once

#include <string>

struct User
{
    int         id;
    std::string name;
};
```

## What it shows

- Wiring `BaldrOpenApiExtension` on the builder via `.WithExtension<...>([](auto& ext){ ... })` and configuring `OpenApiOptions` (title / version / description).
- Using the fluent `RouteRegistration` API: `.WithSummary`, `.WithTag`, `.Handle(...)`.
- Grouping routes under a common prefix with `app->MapGroup("/api/v1", [](auto& group){ ... })`.
- Returning a `std::variant` of typed results from the `users/:id` handler (`JsonResult` / `BadRequestResult` / `NotFoundResult`) — the OpenAPI extension reflects on the success branch and emits the response schema.
- Mounting `baldr::MapScalarUi(*app)` to expose the Scalar UI alongside the auto-generated spec.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/OpenApiExample
```

In another terminal:

```bash
curl http://localhost:8080/openapi.json | jq '.paths, .components.schemas'
# Scalar UI is served at /scalar (open in a browser)
```

The document contains:

- `GET /api/v1/users` — summary `Fetch users`, tagged `users`, response schema `$ref` to `User`.
- `GET /api/v1/users/:id` — summary `Get a user by id`, tagged `users`, response schema `$ref` to `User`.
- `components.schemas.User` — derived from the C++ struct.

## Next steps

- See [OpenAPI extension](../../extensions/openapi.md) for the full options reference, including path templating and JSON Schema dialect limitations.
- See [Route options](../../usage/route-options.md) for the full list of metadata setters.
- Browse [all examples](../examples.md).
