cmake_minimum_required(VERSION 3.28)

project(Baldr VERSION 0.1.0 LANGUAGES C CXX)

option(BALDR_BUILD_EXAMPLES OFF)
option(BALDR_BUILD_TESTS OFF)
option(BALDR_USE_CCACHE "Use ccache for compilation when available" ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (BALDR_USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if (CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  else ()
    message(STATUS "ccache not found; falling back to default compilers")
  endif ()
endif ()

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
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(trantor)

FetchContent_Declare(
  skirnir
  GIT_REPOSITORY "https://github.com/gilmar-sales/Skirnir.git"
  GIT_TAG        "v0.22.0"
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(skirnir)

FetchContent_Declare(
  zlib
  GIT_REPOSITORY "https://github.com/madler/zlib.git"
  GIT_TAG        "v1.3.2"
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(zlib)
if (NOT TARGET ZLIB::ZLIB)
  add_library(ZLIB::ZLIB ALIAS zlib)
endif ()

file(GLOB_RECURSE BALDR_SOURCES
        "src/**.cpp"
)

add_library(baldr ${BALDR_SOURCES})

add_library(baldr::baldr ALIAS baldr)

target_link_libraries(baldr trantor skirnir::skirnir simdjson::simdjson ZLIB::ZLIB)

target_include_directories(baldr PUBLIC src/)

target_precompile_headers(baldr PRIVATE
    <string>
    <memory>
    <functional>
    <unordered_map>
    <vector>
    <utility>
    <Baldr/Baldr.hpp>
)

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