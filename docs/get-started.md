# Get started

Baldr is a C++ microframework for HTTP servers. It provides routing, dependency injection, and middleware, and is consumed in your project via CMake's `FetchContent`.

This page walks you through the prerequisites, a minimal CMake integration, and a complete Hello World program.

## Prerequisites

Before installing Baldr, make sure you have:

- A C++ compiler with **C++26** support — GCC 16+. The CI build matrix pins `gcc-16` on Linux (Clang and MSVC are not part of the matrix because neither implements C++26 reflection); `gcc-15` and older may fail to compile the codebase.
- **CMake 3.28** or newer.
- **Git** — required by CMake's `FetchContent`.

Baldr transitively fetches [Skirnir](https://github.com/gilmar-sales/Skirnir) and [trantor](https://github.com/an-tao/trantor), so an internet connection is required on the first configure.

!!! note "Platform support"
    Baldr builds and runs on Linux, macOS, and Windows. The examples in this documentation assume a POSIX-like environment, but the CMake setup is identical on all platforms.

## Installation with CMake

Add Baldr to your project by declaring it via `FetchContent` in your `CMakeLists.txt`:

```cmake title="CMakeLists.txt"
include(FetchContent)

FetchContent_Declare(
  baldr
  GIT_REPOSITORY "https://github.com/gilmar-sales/Baldr.git"
  GIT_TAG        "main"
)
FetchContent_MakeAvailable(baldr)
```

Then link the `baldr` library to your executable:

```cmake title="CMakeLists.txt"
add_executable(my_app src/main.cpp)

target_link_libraries(my_app PRIVATE baldr)

set_target_properties(my_app PROPERTIES
  CXX_STANDARD 26
  CXX_STANDARD_REQUIRED ON
)
```

!!! tip "Pin to a release tag"
    In production code, replace `GIT_TAG "main"` with a specific tag (for example `v0.16.0`) so your builds are reproducible.

## Hello World

Create a `src/main.cpp` with the following content:

```cpp title="src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

struct Payload
{
    std::string message;
};

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();

    auto app = builder.Build<WebApplication>();

    app->MapGet("/json",
                [&] { return Payload { .message = "Hello, World!" }; });

    app->Run();

    return 0;
}
```

Configure, build, and run:

```bash
cmake -S . -B build
cmake --build build --config Release
./build/my_app
```

Then open another terminal and request the endpoint:

```bash
curl http://localhost:8080/json
```

You should see:

```json
{ "message": "Hello, World!" }
```

## What just happened?

The four lines that make Baldr a server are:

1. `skr::ApplicationBuilder()` — Skirnir's generic application builder.
2. `.WithExtension<BaldrExtension>()` — registers Baldr's services (the router, middleware provider, and HTTP server).
3. `.Build<WebApplication>()` — constructs the strongly-typed application that exposes routing.
4. `app->MapGet("/json", handler)` — registers a handler for `GET /json`. The handler's return value is automatically serialized to JSON.

## Next steps

- Learn the full application lifecycle in [Usage overview](usage/overview.md).
- See a list of runnable examples in [Examples](authoring/examples.md).
- Browse the complete CMake integration in [Build integration](setup/build.md).