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
    auto builder = WebApplication::CreateBuilder();

    auto app = builder.Build();

    app.MapGet("/json", [&] { return Payload { .message = "Hello, World!" }; });

    app.Run();

    return 0;
}
```
