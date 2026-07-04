# Build integration

This page describes how to integrate Baldr into your own CMake project, including the recommended target settings and platform notes.

## Consumer `CMakeLists.txt`

The recommended consumer setup uses `FetchContent`:

```cmake title="CMakeLists.txt"
cmake_minimum_required(VERSION 3.28)

project(MyApp VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
  baldr
  GIT_REPOSITORY "https://github.com/gilmar-sales/Baldr.git"
  GIT_TAG        "main"  # (1)!
)
FetchContent_MakeAvailable(baldr)

add_executable(my_app src/main.cpp)

target_link_libraries(my_app PRIVATE baldr)

set_target_properties(my_app PROPERTIES
  CXX_STANDARD 26
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
```

1. Pin to a specific tag in production code (for example `v0.16.0`).

## Targets exposed

Baldr defines a single library target, `baldr`, with the alias `baldr::baldr`. Both names link to the same library.

```cmake
target_link_libraries(my_app PRIVATE baldr)        # works
target_link_libraries(my_app PRIVATE baldr::baldr)  # also works
```

The target sets `CXX_STANDARD 26` internally. When you link against `baldr`, you should set your own consumer target to **C++26** as well.

## Compile features

The following compile features are required:

- **C++26** — for concepts, `<format>`, designated initializers, `simdjson`, and `std::meta::info` reflection (used by the [OpenAPI extension](../extensions/openapi.md) to auto-derive JSON Schemas from return types via `src/Baldr/OpenApi/JsonSchemaEmitter.cpp`).
- A compiler with **C++26 reflection support** is required to build Baldr from source. CI installs `gcc-14`; Clang 17+ with libstdc++ 14+ works; MSVC 19.40+ works.

## Transitive dependencies

`FetchContent_MakeAvailable(baldr)` also fetches:

- [trantor](https://github.com/an-tao/trantor) — networking primitives.
- [Skirnir](https://github.com/gilmar-sales/Skirnir) — DI container and application builder.

You don't need to declare these yourself — Baldr links them in and propagates their include directories to consumers.

In addition, `CompressionMiddleware` requires **zlib**. Baldr calls `find_package(ZLIB QUIET)` and aborts with a `FATAL_ERROR` if it is missing, so install the development package on your platform (see [Get started](../get-started.md#prerequisites)).

## Platform notes

### Linux

- GCC 14+ or Clang 17+ recommended (CI uses `gcc-14`).
- Install `zlib1g-dev` (Debian/Ubuntu) or `zlib-devel` (Fedora/RHEL) for the compression middleware.

### macOS

- Apple Clang 17+ (Xcode 16) or the official LLVM Clang 17+.
- No additional system packages are required (zlib ships with the platform SDK).

### Windows

- MSVC 19.40+ (Visual Studio 2022 17.10+) or the latest MSVC Build Tools.
- zlib is provided by vcpkg or the platform — install `zlib` via vcpkg or use the version that ships with the Windows SDK.
- Use `cmake -S . -B build` from a **Developer Command Prompt for VS** or a normal shell if `cl.exe` is on `PATH`.

## Disabling examples and tests

When Baldr is the top-level project (`add_subdirectory(baldr)`), it builds examples and tests by default. Through `FetchContent` these options are forced off, so you don't need to do anything.

If you add Baldr as a subdirectory directly, you can opt out:

```cmake
set(BALDR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BALDR_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
add_subdirectory(baldr)
```