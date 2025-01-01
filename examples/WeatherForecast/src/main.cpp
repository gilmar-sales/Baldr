#include <chrono>

#include <Baldr/Baldr.hpp>
#include <random>

struct WeatherForecast
{
    std::string date;
    std::string summary;

    int temperatureC;
    int temperatureF;
};

int random(const int min, const int max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

int main()
{
    auto builder = WebApplication::CreateBuilder();

    auto app = builder.Build();
    auto summaries = std::vector
    {
        "Freezing", "Bracing", "Chilly", "Cool", "Mild", "Warm", "Balmy", "Hot", "Sweltering", "Scorching"
    };

    app.MapGet("/", [&](HttpResponse& response) {
        response.body       = "Hello, World!";
        response.statusCode = StatusCode::OK;

        auto forecast = std::vector<WeatherForecast>(5);

        for (auto& item : forecast)
        {
            const auto celsius = random(-22, 55);

            item = WeatherForecast {
                .date         = "01/01/2025",
                .summary = summaries[random(0, summaries.size()-1)],
                .temperatureC = celsius,
                .temperatureF = 32 + static_cast<int>(celsius / 0.5556)
            };
        }

        return std::move(forecast);
    });

    app.Run();

    return 0;
}
