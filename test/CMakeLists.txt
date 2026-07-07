FetchContent_Declare(
  gtest
  GIT_REPOSITORY "https://github.com/google/googletest"
  GIT_TAG        "v1.17.0"
)
FetchContent_MakeAvailable(gtest)

file(GLOB_RECURSE TESTS_SRC
        "src/**.cpp"
)

add_executable(Tests_run ${TESTS_SRC})

target_link_libraries(Tests_run gtest gtest_main baldr::baldr)
target_include_directories(Tests_run PUBLIC ${gtest_SOURCE_DIR}/include ../include)
target_precompile_headers(Tests_run
   PUBLIC
      <Skirnir/Skirnir.hpp>
      <gtest/gtest.h>
      <Baldr/Baldr.hpp>
)

set_target_properties(Tests_run PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 4
)

gtest_discover_tests(Tests_run)
