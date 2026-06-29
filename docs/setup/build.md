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
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
```

1. Pin to a specific tag in production code (for example `v0.15.1`).

## Targets exposed

Baldr defines a single library target, `baldr`, with the alias `baldr::baldr`. Both names link to the same library.

```cmake
target_link_libraries(my_app PRIVATE baldr)        # works
target_link_libraries(my_app PRIVATE baldr::baldr)  # also works
```

The target sets `CXX_STANDARD 26` internally. When you link against `baldr`, you should set your own consumer target to at least **C++20**.

## Compile features

The following compile features are required:

- **C++20** — for concepts, `<format>`, designated initializers, and `simdjson`.
- A compiler that supports `<meta>` and `std::meta::info` reflection is **not** required for consumers; it is used internally by `LoggingMiddleware` and is compiled only when Baldr itself is built.

## Transitive dependencies

`FetchContent_MakeAvailable(baldr)` also fetches:

- [trantor](https://github.com/an-tao/trantor) — networking primitives.
- [Skirnir](https://github.com/gilmar-sales/Skirnir) — DI container and application builder.

You don't need to declare these yourself — Baldr links them in and propagates their include directories to consumers.

## Platform notes

### Linux

- GCC 11+ or Clang 14+ recommended.
- No additional system packages are required beyond a C++ toolchain, CMake, and Git.

### macOS

- Apple Clang 14+ (Xcode 15) or the official LLVM Clang 14+.
- No additional system packages are required.

### Windows

- MSVC 19.30+ (Visual Studio 2022 17.0+) or the latest MSVC Build Tools.
- Use `cmake -S . -B build` from a **Developer Command Prompt for VS** or a normal shell if `cl.exe` is on `PATH`.

## Disabling examples and tests

When Baldr is the top-level project (`add_subdirectory(baldr)`), it builds examples and tests by default. Through `FetchContent` these options are forced off, so you don't need to do anything.

If you add Baldr as a subdirectory directly, you can opt out:

```cmake
set(BALDR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BALDR_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
add_subdirectory(baldr)
```