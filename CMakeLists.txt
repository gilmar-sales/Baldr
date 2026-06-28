cmake_minimum_required(VERSION 3.28)

project(Baldr VERSION 0.15.1 LANGUAGES C CXX)

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
  GIT_TAG        "refactor/cpp-reflection"
)
FetchContent_MakeAvailable(skirnir)

file(GLOB_RECURSE BALDR_SOURCES
        src/*.cpp
)

add_library(baldr ${BALDR_SOURCES})

add_library(baldr::baldr ALIAS baldr)

target_link_libraries(baldr trantor skirnir::skirnir)

target_include_directories(baldr PUBLIC src/)

if (BALDR_BUILD_EXAMPLES)
  add_subdirectory(examples/WeatherForecast)
  add_subdirectory(examples/HelloWorld)
  add_subdirectory(examples/HelloService)
  add_subdirectory(examples/Devices)
endif ()


if (BALDR_BUILD_TESTS)
  enable_testing()
  include(GoogleTest)
  add_subdirectory(test/)
endif()
