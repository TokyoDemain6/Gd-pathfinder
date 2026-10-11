cmake_minimum_required(VERSION 3.20)
project(gdpf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(GDPF_SOURCES
  src/codec.cpp
  src/level.cpp
  src/objects.cpp
  src/analyze.cpp
  src/sim.cpp
  src/trace.cpp
)

# Warnings, plus the one flag the physics depends on.
function(gdpf_compile_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    # Never merge "a * b + c" into one fused multiply-add. Clang does that by
    # default wherever the CPU has the instruction (every arm64 build), which
    # changes the last bit of the result and so makes the same inputs give a
    # different run on a Mac or a phone than on a PC. The "bit-identical"
    # check in bench/ fails without this.
    target_compile_options(${target} PRIVATE -ffp-contract=off)
  endif()
endfunction()

add_library(gdpf STATIC ${GDPF_SOURCES})
target_include_directories(gdpf PUBLIC include)
gdpf_compile_options(gdpf)

# The command-line tool, the tests and the benchmark are only built when this
# is the top-level project, not when the library is pulled into another build
# (the Geode mod).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(GDPF_STANDALONE ON)
else()
  set(GDPF_STANDALONE OFF)
endif()
option(GDPF_BUILD_TOOLS "Build the gdpf command-line tool" ${GDPF_STANDALONE})
option(GDPF_BUILD_TESTS "Build the gdpf tests" ${GDPF_STANDALONE})
option(GDPF_BUILD_BENCH "Build the benchmark behind the performance checks" ${GDPF_STANDALONE})

if(GDPF_BUILD_TOOLS)
  add_executable(gdpf_cli tools/gdpf_cli.cpp)
  set_target_properties(gdpf_cli PROPERTIES OUTPUT_NAME gdpf)
  target_link_libraries(gdpf_cli PRIVATE gdpf)
endif()

if(GDPF_BUILD_TESTS)
  enable_testing()
  foreach(t codec level sim modformat)
    add_executable(test_${t} tests/test_${t}.cpp)
    target_link_libraries(test_${t} PRIVATE gdpf)
    target_compile_definitions(test_${t} PRIVATE
      GDPF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME ${t} COMMAND test_${t})
  endforeach()
  # The mod's game-independent text helpers are tested here too.
  target_include_directories(test_modformat PRIVATE mod/src)
endif()

if(GDPF_BUILD_BENCH)
  # The same library with collision-work counters compiled in. Kept apart so
  # the counting never slows down the library that gets timed.
  add_library(gdpf_stats STATIC ${GDPF_SOURCES})
  target_include_directories(gdpf_stats PUBLIC include)
  target_compile_definitions(gdpf_stats PUBLIC GDPF_STATS)
  gdpf_compile_options(gdpf_stats)

  foreach(variant "" "_stats")
    add_executable(gdpf_bench${variant} bench/bench.cpp)
    target_link_libraries(gdpf_bench${variant} PRIVATE gdpf${variant})
    target_compile_definitions(gdpf_bench${variant} PRIVATE
      GDPF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    gdpf_compile_options(gdpf_bench${variant})
  endforeach()
endif()
