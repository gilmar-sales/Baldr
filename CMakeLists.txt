cmake_minimum_required(VERSION 3.30)
project(Server)

find_package(asio)
find_package(skirnir)

set(CMAKE_CXX_STANDARD 23)

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
        StatusCode.hpp)

target_link_libraries(Server asio::asio skirnir::skirnir)
