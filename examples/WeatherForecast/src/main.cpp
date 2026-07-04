#include <Baldr/Baldr.hpp>
#include <Baldr/Middleware/RateLimit/Middleware.hpp>
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
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet("/")
        .WithResponseSchemaJson(
            "{\"type\":\"array\",\"items\":{\"type\":\"object\","
            "\"properties\":{\"date\":{\"type\":\"string\"},"
            "\"summary\":{\"type\":\"string\"},"
            "\"temperatureC\":{\"type\":\"integer\"},"
            "\"temperatureF\":{\"type\":\"integer\"}},"
            "\"required\":[\"date\",\"summary\",\"temperatureC\","
            "\"temperatureF\"]}}")
        .Handle([] {
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

    app->Run();

    return 0;
}
