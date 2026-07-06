#include <Baldr/Baldr.hpp>

#include <optional>
#include <variant>

#include "Device.hpp"

namespace
{
    std::vector<Device> makeDevices()
    {
        return std::vector<Device> {
            Device { .id         = 1,
                     .uuid       = "9add349c-c35c-4d32-ab0f-53da1ba40a2a",
                     .mac        = "EF-2B-C4-F5-D6-34",
                     .firmware   = "2.1.5",
                     .created_at = "2024-05-28T15:21:51.137Z",
                     .updated_at = "2024-05-28T15:21:51.137Z" },
            Device { .id         = 2,
                     .uuid       = "d2293412-36eb-46e7-9231-af7e9249fffe",
                     .mac        = "E7-34-96-33-0C-4C",
                     .firmware   = "1.0.3",
                     .created_at = "2024-01-28T15:20:51.137Z",
                     .updated_at = "2024-01-28T15:20:51.137Z" },
            Device { .id         = 3,
                     .uuid       = "eee58ca8-ca51-47a5-ab48-163fd0e44b77",
                     .mac        = "68-93-9B-B5-33-B9",
                     .firmware   = "4.3.1",
                     .created_at = "2024-08-28T15:18:21.137Z",
                     .updated_at = "2024-08-28T15:18:21.137Z" },
            Device { .id         = 4,
                     .uuid       = "ab4efcd0-f542-4944-9dd9-0ad844dfcbd3",
                     .mac        = "E7-6F-69-99-F1-ED",
                     .firmware   = "6.2.0",
                     .created_at = "2024-08-29T15:18:21.137Z",
                     .updated_at = "2024-08-29T15:18:21.137Z" },
            Device { .id         = 5,
                     .uuid       = "9e725cbc-2c4e-446c-a274-962531f90927",
                     .mac        = "9F-57-E5-1F-F5-6B",
                     .firmware   = "0.6.4",
                     .created_at = "2024-18-28T15:18:21.137Z",
                     .updated_at = "2024-18-28T15:18:21.137Z" },
        };
    }

    std::optional<Device> findById(int id)
    {
        for (const auto& d : makeDevices())
        {
            if (d.id == id)
                return d;
        }
        return std::nullopt;
    }
} // namespace

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet("/api/devices", []() { return makeDevices(); });

    app->MapGet("/api/devices/:id")
        .WithSummary("Get a device by id")
        .Handle([](baldr::HttpRequest& request)
                    -> std::variant<
                        baldr::JsonResult<Device, baldr::StatusCode::OK>,
                        baldr::BadRequestResult,
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

            auto found = findById(id);
            if (!found)
                return baldr::Results::NotFound();

            return baldr::Results::Json<Device, baldr::StatusCode::OK>(*found);
        });

    app->Run();

    return 0;
}