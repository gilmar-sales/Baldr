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