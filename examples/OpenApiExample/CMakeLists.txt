add_executable(OpenApiExample src/main.cpp)

target_include_directories(OpenApiExample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(OpenApiExample
    baldr::baldr
)