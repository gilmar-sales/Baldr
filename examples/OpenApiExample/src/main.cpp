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
            .Handle(
                [](baldr::HttpRequest& request)
                    -> std::variant<baldr::JsonResult, baldr::BadRequestResult,
                                    baldr::NotFoundResult> {
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

                    return baldr::Results::Json(*found);
                });
    });

    baldr::MapScalarUi(*app);

    app->Run();

    return 0;
}