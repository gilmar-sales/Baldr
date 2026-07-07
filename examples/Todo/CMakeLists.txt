add_executable(Todo
    src/main.cpp
    src/InMemoryTodoRepository.cpp
    src/TodoController.cpp
)

target_link_libraries(Todo baldr)