cmake_minimum_required(VERSION 3.30)
project(Server)

set(CMAKE_CXX_STANDARD 23)

find_package(asio)
find_package(skirnir)
find_package(reflectcpp)

add_executable(Server main.cpp
        WebApplication.cpp
        WebApplication.hpp
        WebApplicationBuilder.cpp
        WebApplicationBuilder.hpp
        HttpServer.hpp
        HttpSession.hpp
        HttpRequest.hpp
        HttpResponse.hpp
        CookieOptions.hpp
        IMiddleware.hpp
        PathMatcher.cpp
        PathMatcher.hpp
        Tuple.hpp
        RateLimitMiddleware.h
        StatusCode.hpp
        LoggingMiddleware.hpp)

target_link_libraries(Server asio::asio skirnir::skirnir reflectcpp::reflectcpp)
