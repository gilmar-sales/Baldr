# Getting started

## With CMake

```cmake
include(FetchContent)

FetchContent_Declare(
  baldr
  GIT_REPOSITORY "https://github.com/gilmar-sales/Baldr.git"
  GIT_TAG        "main"
)
FetchContent_MakeAvailable(baldr)
```

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


