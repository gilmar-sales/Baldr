# OpenAPI example

[`examples/OpenApiExample`](https://github.com/gilmar-sales/Baldr/tree/main/examples/OpenApiExample) shows the [`BaldrOpenApiExtension`](../../extensions/openapi.md) end-to-end: route metadata (`WithSummary`, `WithOperationId`, `WithTag`) feeds an auto-generated OpenAPI 3.0.3 document served at `/openapi.json`.

## Source

[`examples/OpenApiExample/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/OpenApiExample/src/main.cpp):

```cpp title="examples/OpenApiExample/src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

#include "Device.hpp"
#include "User.hpp"

int main()
{
    auto builder =
        skr::ApplicationBuilder()
            .WithExtension<BaldrExtension>()
            .WithExtension<BaldrOpenApiExtension>(
                [](BaldrOpenApiExtension& openApi) {
                    OpenApiOptions opts;
                    opts.info.title       = "Devices API";
                    opts.info.version     = "1.0.0";
                    opts.info.description = "Reference example demonstrating "
                                            "RouteOptions + OpenAPI 3.0.3.";
                    openApi.WithOptions(opts);
                });

    auto app = builder.Build<WebApplication>();

    app->MapGet("/api/devices")
        .WithSummary("List devices")
        .WithOperationId("listDevices")
        .WithTag("devices")
        .Handle([]() {
            return std::vector<Device> {
                Device { 1, "9add349c-c35c-4d32-ab0f-53da1ba40a2a",
                         "EF-2B-C4-F5-D6-34", "2.1.5",
                         "2024-05-28T15:21:51.137Z",
                         "2024-05-28T15:21:51.137Z" },
                Device { 2, "d2293412-36eb-46e7-9231-af7e9249fffe",
                         "E7-34-96-33-0C-4C", "1.0.3",
                         "2024-01-28T15:20:51.137Z",
                         "2024-01-28T15:20:51.137Z" },
            };
        });

    app->MapGroup("/api/v1", [](auto& group) {
        group.MapGet("/users")
            .WithSummary("Fetch users")
            .WithTag("users")
            .Handle([](HttpRequest& req) {
                return std::vector<User> { User { .id = 1, .name = "First" },
                                           User { .id = 2, .name = "Second" } };
            });
    });

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
    int          id;
    std::string  uuid;
    std::string  mac;
    std::string  firmware;
    std::string  created_at;
    std::string  updated_at;
};
```

[`examples/OpenApiExample/src/User.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/OpenApiExample/src/User.hpp):

```cpp title="User.hpp"
#pragma once

#include <string>

struct User
{
    int          id;
    std::string  name;
};
```

## What it shows

- Wiring `BaldrOpenApiExtension` on the builder via `.WithExtension<...>([](auto& ext){ ... })` and configuring `OpenApiOptions`.
- Using the fluent `RouteRegistration` API: `.WithSummary`, `.WithOperationId`, `.WithTag`, `.Handle(...)`.
- Returning a `std::vector<Device>` and a `std::vector<User>` from handlers. The OpenAPI extension introspects the return type with C++26 reflection and emits a draft-07 JSON Schema per DTO into `components.schemas`.
- Grouping routes under a common prefix with `app->MapGroup("/api/v1", [](auto& group){ ... })`.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/OpenApiExample
```

In another terminal:

```bash
curl http://localhost:8080/openapi.json | jq '.paths, .components.schemas'
```

The document contains:

- `GET /api/devices` — operation id `listDevices`, tagged `devices`, response schema `$ref` to `Device`.
- `GET /api/v1/users` — tagged `users`, response schema `$ref` to `User`.
- `components.schemas.Device` and `components.schemas.User` — derived from the C++ structs.

## Next steps

- See [OpenAPI extension](../../extensions/openapi.md) for the full options reference, including path templating and JSON Schema dialect limitations.
- See [Route options](../../usage/route-options.md) for the full list of metadata setters.
- Browse [all examples](../examples.md).