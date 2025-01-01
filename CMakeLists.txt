cmake_minimum_required(VERSION 3.30)

project(Baldr)

option(BALDR_BUILD_EXAMPLES OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(asio)
find_package(skirnir)
find_package(reflectcpp)

add_library(baldr
        src/WebApplication.cpp
        src/WebApplicationBuilder.cpp
        src/PathMatcher.cpp)

target_link_libraries(baldr asio::asio skirnir::skirnir reflectcpp::reflectcpp)

target_include_directories(baldr PUBLIC include/)

if (BALDR_BUILD_EXAMPLES)
    add_subdirectory(examples/WeatherForecast)
endif ()