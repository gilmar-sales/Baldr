set (SQLGEN_SQLITE3 OFF)
FetchContent_Declare(
  sqlgen
  GIT_REPOSITORY "https://github.com/gilmar-sales/sqlgen.git"
  GIT_TAG        "main"
)
FetchContent_MakeAvailable(sqlgen)

file(GLOB_RECURSE SQLGEN_INTEGRATION_SOURCES
        src/*.cpp
)

add_executable(SqlgenIntegration ${SQLGEN_INTEGRATION_SOURCES})

target_link_libraries(SqlgenIntegration baldr sqlgen)