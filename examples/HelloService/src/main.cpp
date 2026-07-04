#include <Baldr/Baldr.hpp>

#include "HelloService.hpp"

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    builder.GetServiceCollection()->AddTransient<HelloService>();

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet(
        "/hello/:name",
        [](skr::Arc<HelloService> helloService, baldr::HttpRequest& request) {
            return helloService->Hello(request.params["name"]);
        });

    app->Run();

    return 0;
}