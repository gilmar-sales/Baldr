#include <Baldr/Baldr.hpp>

#include <variant>

#include "HelloService.hpp"

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    builder.GetServiceCollection()->AddTransient<HelloService>();

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet(
        "/hello/:name",
        [](skr::Arc<HelloService> helloService, baldr::HttpRequest& request)
            -> std::variant<Payload, baldr::BadRequestResult> {
            const auto& name = request.params["name"];
            if (name.empty())
                return baldr::Results::BadRequest("name is required");

            return helloService->Hello(name);
        });

    app->Run();

    return 0;
}