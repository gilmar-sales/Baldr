# Weather forecast example

[`examples/WeatherForecast`](https://github.com/gilmar-sales/Baldr/tree/main/examples/WeatherForecast) generates a random weather forecast and returns it as JSON.

## Source

[`examples/WeatherForecast/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/WeatherForecast/src/main.cpp):

```cpp title="examples/WeatherForecast/src/main.cpp" linenums="1"
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

## What it shows

- Returning a `std::vector<Aggregate>` where each item is initialised with designated initializers.
- Using `std::string_view` fields that serialise cleanly to JSON without copying.
- Composing simple computation in the handler without any state.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/WeatherForecast
```

In another terminal:

```bash
curl http://localhost:8080/
```

## Next steps

- Browse [all examples](../examples.md).