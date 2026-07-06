#include <Baldr/Baldr.hpp>

#include "InMemoryTodoRepository.hpp"
#include "TodoController.hpp"

int main()
{
    auto builder = skr::ApplicationBuilder()
                       .WithExtension<baldr::BaldrExtension>()
                       .WithExtension<baldr::BaldrOpenApiExtension>();

    builder.GetServiceCollection()
        ->AddSingleton<ITodoRepository, InMemoryTodoRepository>();

    auto app = builder.Build<baldr::WebApplication>();

    baldr::MapScalarUi(*app);

    TodoController controller(
        app->GetRootServiceProvider()->GetService<ITodoRepository>());
    controller.Register(*app);

    app->Run();

    return 0;
}