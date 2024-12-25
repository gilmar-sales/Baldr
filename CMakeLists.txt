cmake_minimum_required(VERSION 3.30)
project(Server)

find_package(asio)
find_package(Skirnir)

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
        PathMatcher.hpp)

target_link_libraries(Server mswsock ws2_32 asio::asio skirnir::skirnir)
