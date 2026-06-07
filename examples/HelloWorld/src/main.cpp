#include <Baldr/Baldr.hpp>

struct Payload
{
    std::string message;
};

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();

    auto app = builder.BuildAsync<WebApplication>();

    app->MapGet("/json",
                [&] { return Payload { .message = "Hello, World!" }; });

    skr::AsyncApplicationHost::Run(*app);

    return 0;
}