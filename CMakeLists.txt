cmake_minimum_required(VERSION 3.28)

project(Baldr VERSION 0.16.0 LANGUAGES C CXX)

option(BALDR_BUILD_EXAMPLES OFF)
option(BALDR_BUILD_TESTS OFF)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  set(BALDR_BUILD_EXAMPLES ON)
  set(BALDR_BUILD_TESTS ON)
endif ()

include(FetchContent)


set(TRANTOR_BUILD_C-ARES OFF CACHE BOOL "" FORCE)
set(TRANTOR_USE_TLS "none" CACHE STRING "" FORCE)
FetchContent_Declare(
  trantor
  GIT_REPOSITORY "https://github.com/an-tao/trantor.git"
  GIT_TAG        "v1.5.28"
)
FetchContent_MakeAvailable(trantor)

FetchContent_Declare(
  skirnir
  GIT_REPOSITORY "https://github.com/gilmar-sales/Skirnir.git"
  GIT_TAG        "v0.22.0"
)
FetchContent_MakeAvailable(skirnir)

# CompressionMiddleware uses libz (zlib) for gzip. AGENTS.md prefers
# FetchContent for external dependencies, but zlib is small and
# ubiquitous; locate it on the host system first.
find_package(ZLIB QUIET)

# Explicit source list grouped by domain. Mirrors the directory layout
# under src/Baldr/. Prefer adding files here over GLOB so CMake
# re-runs reliably.
set(BALDR_SOURCES
    # Top-level
    src/Baldr/BaldrExtension.cpp

    # Http/
    src/Baldr/Http/Connection.cpp
    src/Baldr/Http/RequestParser.cpp
    src/Baldr/Http/Router.cpp
    src/Baldr/Http/Server.cpp

    # Middleware/Compression/
    src/Baldr/Middleware/Compression/Internal.cpp
    src/Baldr/Middleware/Compression/Middleware.cpp

    # Metrics/
    src/Baldr/Metrics/Registry.cpp
    src/Baldr/Metrics/Middleware.cpp

    # Application/
    src/Baldr/Application/WebApplication.cpp

    # OpenApi/
    src/Baldr/OpenApi/JsonSchemaEmitter.cpp
    src/Baldr/OpenApi/OpenApiSpecService.cpp
    src/Baldr/OpenApi/RouteIntrospector.cpp
    src/Baldr/OpenApi/SpecBuilder.cpp
)

add_library(baldr ${BALDR_SOURCES})

add_library(baldr::baldr ALIAS baldr)

target_link_libraries(baldr trantor skirnir::skirnir simdjson::simdjson)
if (ZLIB_FOUND)
  target_link_libraries(baldr ZLIB::ZLIB)
else ()
  message(FATAL_ERROR "zlib was not found; CompressionMiddleware requires it")
endif ()

target_include_directories(baldr PUBLIC src/)

# Precompiled headers: stable, ubiquitous STL headers + the umbrella
# Baldr header are reused across most translation units. Cuts the
# first-byte cost of every .cpp by ~50-100 ms on this codebase.
target_precompile_headers(baldr PRIVATE
    <string>
    <memory>
    <functional>
    <unordered_map>
    <vector>
    <utility>
    <Baldr/Baldr.hpp>
)

# Unity builds: collapse small groups of .cpp files into a single TU,
# sidestepping per-TU include-graph work. A batch size of 4 keeps
# incremental rebuilds narrow while still cutting first-build cost.
set_target_properties(baldr PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 4
)

if (BALDR_BUILD_EXAMPLES)
  add_subdirectory(examples/WeatherForecast)
  add_subdirectory(examples/HelloWorld)
  add_subdirectory(examples/HelloService)
  add_subdirectory(examples/Devices)
  add_subdirectory(examples/StaticFiles)
  add_subdirectory(examples/FileStream)
  add_subdirectory(examples/OpenApiExample)
endif ()


if (BALDR_BUILD_TESTS)
  enable_testing()
  include(GoogleTest)
  add_subdirectory(test/)
endif()