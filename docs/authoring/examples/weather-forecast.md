# Weather forecast example

[`examples/WeatherForecast`](https://github.com/gilmar-sales/Baldr/tree/main/examples/WeatherForecast) generates a random weather forecast, returns it as JSON, and exposes the same surface as an OpenAPI 3.0.3 document and Scalar UI.

## Source

[`examples/WeatherForecast/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/WeatherForecast/src/main.cpp):

```cpp title="examples/WeatherForecast/src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

#include <random>

struct WeatherForecast
{
    std::string      date;
    std::string_view summary;

    int temperatureC;
    int temperatureF;
};

int random(const int min, const int max)
{
    std::random_device              rd;
    std::mt19937                    gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

int main()
{
    auto builder = skr::ApplicationBuilder()
                       .WithExtension<baldr::BaldrExtension>()
                       .WithExtension<baldr::BaldrOpenApiExtension>();

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet("/").Handle([] {
        auto forecast = std::vector<WeatherForecast>(5);

        static auto summaries =
            std::vector { "Freezing",   "Bracing",  "Chilly", "Cool",
                          "Mild",       "Warm",     "Balmy",  "Hot",
                          "Sweltering", "Scorching" };

        for (auto& item : forecast)
        {
            const auto celsius = random(-22, 55);

            item = WeatherForecast {
                .date         = "01/01/2025",
                .summary      = summaries[random(0, summaries.size() - 1)],
                .temperatureC = celsius,
                .temperatureF = 32 + static_cast<int>(celsius / 0.5556)
            };
        }

        return forecast;
    });

    baldr::MapOpenApi(*app);
    baldr::MapScalarUi(*app);

    app->Run();

    return 0;
}
```

## What it shows

- Returning a `std::vector<Aggregate>` where each item is initialised with designated initializers.
- Using `std::string_view` fields that serialise cleanly to JSON without copying.
- Composing simple computation in the handler without any state.
- The fluent route API: `MapGet("/").Handle(lambda)` instead of the legacy `MapGet(path, lambda)` overload.
- Wiring the OpenAPI extension with no custom `OpenApiOptions` (defaults are good enough for a single endpoint) and mounting `baldr::MapOpenApi` and `baldr::MapScalarUi` on the same `WebApplication`.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/WeatherForecast
```

In another terminal:

```bash
curl http://localhost:8080/
curl http://localhost:8080/openapi.json | jq '.paths, .components.schemas'
# Scalar UI is served at /scalar (open in a browser)
```

## Next steps

- See [OpenAPI extension](../../extensions/openapi.md) for `OpenApiOptions` and the schema dialect.
- See [Route options](../../usage/route-options.md) for the metadata setters used by the extension.
- Browse [all examples](../examples.md).
