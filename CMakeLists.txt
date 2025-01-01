cmake_minimum_required(VERSION 3.30)
project(Server)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(asio)
find_package(skirnir)
find_package(reflectcpp)

add_executable(Server src/main.cpp
        src/WebApplication.cpp
        src/WebApplicationBuilder.cpp
        src/PathMatcher.cpp)

target_link_libraries(Server asio::asio skirnir::skirnir reflectcpp::reflectcpp)

target_include_directories(Server PUBLIC include/)
