#include <Baldr/Baldr.hpp>

#include "HelloService.hpp"

int main()
{
    auto builder = WebApplication::CreateBuilder();

    builder.GetServiceCollection().AddTransient<HelloService>();

    auto app = builder.Build();

    app.MapGet("/json", [](Ref<HelloService> helloService) {
        return helloService->Hello();
    });

    app.Run();

    return 0;
}