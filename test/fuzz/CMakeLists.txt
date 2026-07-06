# test/fuzz/CMakeLists.txt - afl++ fuzz harness build.
#
# Activated by setting -DBALDR_BUILD_FUZZING=ON (default OFF for downstream
# consumers, ON when Baldr is the top-level project, mirroring the
# BALDR_BUILD_EXAMPLES / BALDR_BUILD_TESTS policy in the root CMakeLists).
#
# afl++ is required. Configure via BALDR_FUZZ_USE_AFL (default ON). When
# OFF the harness compiles as a regular executable so the harness surface
# can still be built locally without afl++ installed.
#
# Each harness links against baldr::baldr only; no gtest, no PCH, no unity
# build (single TU per executable, so a regression isn't masked by the batch).

cmake_minimum_required(VERSION 3.28)

if(NOT BALDR_BUILD_FUZZING)
  return()
endif()

option(BALDR_FUZZ_USE_AFL "Wrap harness compilation with afl++ (default ON)" ON)

# Default flags for a local dev build. The CI workflows override these via
# -DBALDR_FUZZ_CXX_FLAGS / -DBALDR_FUZZ_LINK_FLAGS for each sanitizer lane.
# These must be lists (semicolon-separated in CMake) so they expand into
# multiple compile options.
if(NOT DEFINED BALDR_FUZZ_CXX_FLAGS)
  set(BALDR_FUZZ_CXX_FLAGS "-g;-O1")
endif()
if(NOT DEFINED BALDR_FUZZ_LINK_FLAGS)
  set(BALDR_FUZZ_LINK_FLAGS "")
endif()

set(BALDR_FUZZ_SOURCES
  HttpParserFuzz.cpp
  RouterFuzz.cpp
  MiddlewareFuzz.cpp
  ResultsFuzz.cpp
  CookieFuzz.cpp
  CompressionFuzz.cpp
  StaticFilesFuzz.cpp
  SecurityFuzz.cpp
)

set(BALDR_FUZZ_COMMON_SOURCES
  afl_driver.cpp
)

# Detect whether the global CMAKE_CXX_COMPILER is already an afl++ wrapper.
# The supported way to use afl++ with this project is to set
# -DCMAKE_C_COMPILER=afl-gcc-fast -DCMAKE_CXX_COMPILER=afl-g++-fast at
# configure time (CI does this). The wrapper handles both compile and
# link, and instrumentation is consistent across the harness and the
# library it links against. When the global compiler is *not* an afl
# wrapper, we fall back to building without instrumentation and emit
# a warning so the user knows the fuzzer won't be useful until they
# pass the wrappers.
if(BALDR_FUZZ_USE_AFL)
  if(CMAKE_CXX_COMPILER MATCHES "afl-(g\\+\\+|gcc|g\\+\\+).*")
    message(STATUS "Baldr fuzzing: CMAKE_CXX_COMPILER already uses afl++ "
                    "wrapper (${CMAKE_CXX_COMPILER})")
  else()
    message(WARNING
      "Baldr fuzzing: BALDR_FUZZ_USE_AFL=ON but CMAKE_CXX_COMPILER "
      "(${CMAKE_CXX_COMPILER}) is not an afl++ wrapper. Falling back to "
      "plain gcc so the build succeeds; the harnesses will not be "
      "instrumented. Re-run cmake with:\n"
      "  -DCMAKE_C_COMPILER=afl-gcc-fast -DCMAKE_CXX_COMPILER=afl-g++-fast\n"
      "to enable afl++ fuzzing. Pass -DBALDR_FUZZ_USE_AFL=OFF to silence "
      "this warning.")
    set(BALDR_FUZZ_USE_AFL OFF)
  endif()
else()
  message(STATUS "Baldr fuzzing: building harnesses without afl++ wrapper")
endif()

foreach(src ${BALDR_FUZZ_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} ${BALDR_FUZZ_COMMON_SOURCES})
  target_link_libraries(${name} PRIVATE baldr::baldr)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${name} PRIVATE cxx_std_26)
  # Disable PCH/unity for fuzz targets - we want fast, isolated compilation
  # so that harness-specific diagnostics aren't swallowed by a batch.
  set_target_properties(${name} PROPERTIES
    UNITY_BUILD OFF
  )
  # Explicitly empty PCH list to defeat CMake >= 4.x default-on PCH for
  # executable targets.
  target_precompile_headers(${name} PRIVATE)

  if(BALDR_FUZZ_USE_AFL)
    target_compile_options(${name} PRIVATE ${BALDR_FUZZ_CXX_FLAGS})
    target_link_options(${name} PRIVATE ${BALDR_FUZZ_LINK_FLAGS})
    # afl++ persistent mode is enabled via a compile definition; the driver
    # uses __AFL_LOOP when this is set.
    target_compile_definitions(${name} PRIVATE BALDR_FUZZ_PERSISTENT=1)
  else()
    target_compile_options(${name} PRIVATE ${BALDR_FUZZ_CXX_FLAGS})
    target_link_options(${name} PRIVATE ${BALDR_FUZZ_LINK_FLAGS})
  endif()

  # Register a ctest entry. The wrapper script enforces a timeout and parses
  # afl++'s fuzzer_stats for unique_crashes > 0. Corpus is resolved relative
  # to the source tree so ctest works from any working directory.
  add_test(
    NAME fuzz.${name}
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_one_fuzz.sh
            $<TARGET_FILE:${name}>
            ${CMAKE_SOURCE_DIR}/test/fuzz/corpus/${name}
            90
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(fuzz.${name} PROPERTIES TIMEOUT 180)
endforeach()