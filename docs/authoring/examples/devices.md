# Devices example

[`examples/Devices`](https://github.com/gilmar-sales/Baldr/tree/main/examples/Devices) returns a list of `Device` records.

## Source

[`examples/Devices/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/Devices/src/main.cpp):

```cpp title="examples/Devices/src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

#include "Device.hpp"

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->MapGet("/api/devices", []() {
        auto devices = std::vector<Device> {
            Device {
                .id= 1,
                .uuid= "9add349c-c35c-4d32-ab0f-53da1ba40a2a",
                .mac= "EF-2B-C4-F5-D6-34",
                .firmware= "2.1.5",
            },
            // ...
        };

        return std::move(devices);
    });

    app->Run();
}
```

## What it shows

- Returning a `std::vector<T>` from a handler. The framework serialises the entire vector as a JSON array — no special-casing required.
- Organising the response DTO in a separate header (`Device.hpp`) so the type can be reused.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/Devices
```

In another terminal:

```bash
curl http://localhost:8080/api/devices
```

## Next steps

- See [Route options](../../usage/route-options.md) for adding summary, tags, and an OpenAPI operation id to this route.
- See the [OpenAPI example](open-api.md) for the end-to-end extension that generates a spec from this pattern.
- Browse [all examples](../examples.md).