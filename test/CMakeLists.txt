FetchContent_Declare(
  gtest
  GIT_REPOSITORY "https://github.com/google/googletest"
  GIT_TAG        "main"
)
FetchContent_MakeAvailable(gtest)

enable_testing()
include(GoogleTest)

file(GLOB_RECURSE TESTS_SRC
        "src/**.cpp"
)

add_executable(Tests_run ${TESTS_SRC})

target_link_libraries(Tests_run gtest gtest_main baldr::baldr)
target_include_directories(Tests_run PUBLIC ${gtest_SOURCE_DIR}/include ../include)

gtest_discover_tests(Tests_run)