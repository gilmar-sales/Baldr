#include <Baldr/Baldr.hpp>

#include "HelloService.hpp"

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();

    builder.GetServiceCollection()->AddTransient<HelloService>();

    auto app = builder.Build<WebApplication>();

    app->MapGet("/hello/:name",
                [](skr::Arc<HelloService> helloService, HttpRequest& request) {
                    return helloService->Hello(request.params["name"]);
                });

    app->Run();

    return 0;
}