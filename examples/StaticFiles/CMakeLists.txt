add_executable(StaticFiles src/main.cpp)

target_link_libraries(StaticFiles baldr)

add_custom_command(
  TARGET StaticFiles POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/wwwroot
          $<TARGET_FILE_DIR:StaticFiles>/wwwroot
)
