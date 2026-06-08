cmake_minimum_required(VERSION 3.28)

project(Baldr VERSION 0.15.1)

option(BALDR_BUILD_EXAMPLES OFF)
option(BALDR_BUILD_TESTS OFF)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  set(BALDR_BUILD_EXAMPLES ON)
  set(BALDR_BUILD_TESTS ON)
endif ()

include(FetchContent)

option(ASIO_USE_CPM "Download Asio with CPM instead of FetchContent" OFF)
option(
  ASIO_CPM_FETCHCONTENT_COMPAT
  "Should asio be declared with FetchContent functions to be compatible. This doesn't not allow CPM cache to work."
  ON
)

FetchContent_Declare(
  asiocmake
  GIT_REPOSITORY "https://github.com/gilmar-sales/asio.cmake.git"
  GIT_TAG        "main"
)
FetchContent_MakeAvailable(asiocmake)

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

target_link_libraries(baldr asio::asio skirnir::skirnir)

if (WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  target_link_libraries(baldr ws2_32 wsock32 Bcrypt)
endif()

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
