# Baldr
A C++ microframework for the web with native support for dependency injection and logging
# Baldr

[![CI](https://github.com/gilmar-sales/Baldr/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/gilmar-sales/Baldr/actions/workflows/cmake-multi-platform.yml)
[![Docs](https://img.shields.io/badge/docs-available-blue)](https://gilmar-sales.github.io/Baldr/)
[![License](https://img.shields.io/github/license/gilmar-sales/Baldr)](LICENSE)

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
