#include <Baldr/Baldr.hpp>

#include "HelloService.hpp"

int main()
{
    auto builder = skr::ApplicationBuilder().AddExtension(BaldrExtension());

    builder.GetServiceCollection().AddTransient<HelloService>();

    auto app = builder.Build<WebApplication>();

    app->MapGet("/json", [](Ref<HelloService> helloService) {
        return helloService->Hello();
    });

    app->Run();

    return 0;
}