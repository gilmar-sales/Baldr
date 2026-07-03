#include <Baldr/Baldr.hpp>

#include "Device.hpp"

int main()
{
    auto builder =
        skr::ApplicationBuilder()
            .WithExtension<BaldrExtension>()
            .WithExtension<Baldr::OpenApi::BaldrOpenApiExtension>(
                [](Baldr::OpenApi::BaldrOpenApiExtension& openApi) {
                    Baldr::OpenApi::OpenApiOptions opts;
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
        .WithResponseSchemaJson(
            "{\"type\":\"array\",\"items\":{\"type\":\"object\","
            "\"properties\":{\"id\":{\"type\":\"integer\"},"
            "\"uuid\":{\"type\":\"string\"},"
            "\"mac\":{\"type\":\"string\"},"
            "\"firmware\":{\"type\":\"string\"},"
            "\"created_at\":{\"type\":\"string\"},"
            "\"updated_at\":{\"type\":\"string\"}},"
            "\"required\":[\"id\",\"uuid\",\"mac\",\"firmware\","
            "\"created_at\",\"updated_at\"]}}")
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
        group.MapGet("/users/:id")
            .WithSummary("Fetch user by id")
            .WithTag("users")
            .Handle([](HttpRequest& req) {
                return std::string { "user " + req.params["id"] };
            });
    });

    app->Run();

    return 0;
}