cmake_minimum_required(VERSION 3.28)

project(Baldr)

option(BALDR_BUILD_EXAMPLES OFF)
option(BALDR_BUILD_TESTS OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

option(ASIO_USE_CPM "Download Asio with CPM instead of FetchContent" OFF)
option(
  ASIO_CPM_FETCHCONTENT_COMPAT
  "Should asio be declared with FetchContent functions to be compatible. This doesn't not allow CPM cache to work."
  ON
)

FetchContent_Declare(
  asiocmake
  GIT_REPOSITORY "https://github.com/OlivierLDff/asio.cmake"
  GIT_TAG        "main"
)
FetchContent_MakeAvailable(asiocmake)

FetchContent_Declare(
  skirnir
  GIT_REPOSITORY "https://github.com/gilmar-sales/Skirnir.git"
  GIT_TAG        "v0.12.1"
)
FetchContent_MakeAvailable(skirnir)

FetchContent_Declare(
  reflectcpp
  GIT_REPOSITORY "https://github.com/getml/reflect-cpp.git"
  GIT_TAG        "main"
  OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(reflectcpp)

add_library(reflectcpp::reflectcpp ALIAS reflectcpp)

message(STATUS "Using Skirnir version: ")

file(GLOB_RECURSE BALDR_SOURCES
        src/*.cpp
)

add_library(baldr ${BALDR_SOURCES})

add_library(baldr::baldr ALIAS baldr)

target_link_libraries(baldr asio::asio skirnir::skirnir reflectcpp)

if (WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
target_link_libraries(baldr ws2_32 wsock32 Bcrypt)

endif()

target_include_directories(baldr PUBLIC src/)

if (BALDR_BUILD_EXAMPLES)
    add_subdirectory(examples/WeatherForecast)
    add_subdirectory(examples/HelloWorld)
    add_subdirectory(examples/HelloService)
    add_subdirectory(examples/Devices)
    add_subdirectory(examples/SqlgenIntegration)
endif ()


if (BALDR_BUILD_TESTS)
add_subdirectory(./test)
endif()
