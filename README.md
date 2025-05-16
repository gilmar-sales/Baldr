# Baldr
A C++ microframework for the web with native support for dependency injection and logging

# Usage

## Basic web applications
```cpp
#include <Baldr/Baldr.hpp>

struct Payload
{
    std::string message;
};

int main()
{
    auto builder = skr::ApplicationBuilder().AddExtension(BaldrExtension());

    auto app = builder.Build<WebApplication>();

    app->MapGet("/json",
                [&] { return Payload { .message = "Hello, World!" }; });

    app->Run();

    return 0;
}
```
