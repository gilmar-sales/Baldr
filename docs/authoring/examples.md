# Examples

The [`examples/`](https://github.com/gilmar-sales/Baldr/tree/main/examples) directory contains small, runnable programs that demonstrate individual features of Baldr. Each example is a self-contained CMake target.

## Hello World

[`examples/HelloWorld`](https://github.com/gilmar-sales/Baldr/tree/main/examples/HelloWorld) — the smallest possible Baldr program.

```cpp title="examples/HelloWorld/src/main.cpp"
#include <Baldr/Baldr.hpp>

struct Payload
{
    std::string message;
};

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();

    auto app = builder.Build<WebApplication>();

    app->MapGet("/json",
                [&] { return Payload { .message = "Hello, World!" }; });

    app->Run();

    return 0;
}
```

**What it shows:** application composition, route registration, automatic JSON serialization of a returned aggregate.

## Hello Service

[`examples/HelloService`](https://github.com/gilmar-sales/Baldr/tree/main/examples/HelloService) — adds a custom service that is injected into a route handler.

```cpp title="examples/HelloService/src/main.cpp"
#include <Baldr/Baldr.hpp>

#include "HelloService.hpp"

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();

    builder.GetServiceCollection()->AddTransient<HelloService>();

    auto app = builder.Build<WebApplication>();

    app->MapGet("/hello/:name",
                [](skr::Arc<HelloService> helloService, HttpRequest& request) {
                    return helloService->Hello(request.params["name"]);
                });

    app->Run();

    return 0;
}
```

**What it shows:** registering a service, declaring handler parameters, reading path parameters, returning a custom type that is serialized to JSON.

## Devices

[`examples/Devices`](https://github.com/gilmar-sales/Baldr/tree/main/examples/Devices) — returns a list of `Device` records.

```cpp title="examples/Devices/src/main.cpp" hl_lines="11"
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

**What it shows:** returning a `std::vector<T>` from a handler, where each `Device` is an aggregate with multiple fields. The framework serializes the entire vector to a JSON array.

## Weather forecast

[`examples/WeatherForecast`](https://github.com/gilmar-sales/Baldr/tree/main/examples/WeatherForecast) — generates a random weather forecast and returns it as JSON.

```cpp title="examples/WeatherForecast/src/main.cpp"
#include <Baldr/Baldr.hpp>

struct WeatherForecast
{
    std::string date;
    std::string_view summary;
    int temperatureC;
    int temperatureF;
};

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->MapGet("/", [] {
        auto forecast = std::vector<WeatherForecast>(5);

        static auto summaries =
            std::vector { "Freezing", "Bracing", "Chilly", "Cool",
                          "Mild", "Warm", "Balmy", "Hot",
                          "Sweltering", "Scorching" };

        for (auto& item : forecast)
        {
            const auto celsius = random(-22, 55);
            item = WeatherForecast {
                .date         = "01/01/2025",
                .summary      = summaries[random(0, summaries.size() - 1)],
                .temperatureC = celsius,
                .temperatureF = 32 + static_cast<int>(celsius / 0.5556),
            };
        }

        return forecast;
    });

    app->Run();
}
```

**What it shows:** generating structured data in a handler, using `std::string_view` fields that serialize cleanly, and returning a populated container.

## Building the examples

When Baldr is the top-level project, examples are built by default:

```bash
cmake -S . -B build
cmake --build build
```

Each example produces an executable named after its directory: `./build/HelloWorld`, `./build/HelloService`, `./build/Devices`, `./build/WeatherForecast`.

To disable examples (for example in a production build), set `-DBALDR_BUILD_EXAMPLES=OFF` at configure time.