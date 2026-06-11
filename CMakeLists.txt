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


FetchContent_Declare(
  h2o
  GIT_REPOSITORY "https://github.com/h2o/h2o.git"
  GIT_TAG        "ed7899d1c234fd15d272a0ac6404ffbd89a3c772"
)
FetchContent_MakeAvailable(h2o)

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

target_link_libraries(baldr libh2o skirnir::skirnir)

if (WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  target_link_libraries(baldr ws2_32 wsock32 Bcrypt)
endif()

target_include_directories(baldr PUBLIC src/)
target_include_directories(baldr SYSTEM PUBLIC
  ${h2o_SOURCE_DIR}/include
  ${h2o_SOURCE_DIR}/deps/picotls/include
  ${h2o_SOURCE_DIR}/deps/quicly/include
  ${h2o_SOURCE_DIR}/deps/yoml
  ${h2o_SOURCE_DIR}/deps/hiredis
  ${h2o_SOURCE_DIR}/deps/klib
  ${h2o_SOURCE_DIR}/deps/picohttpparser
)

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
