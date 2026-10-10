cmake_minimum_required(VERSION 3.20)
project(gdpf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(gdpf STATIC
  src/codec.cpp
  src/level.cpp
  src/objects.cpp
  src/analyze.cpp
  src/sim.cpp
  src/trace.cpp
)
target_include_directories(gdpf PUBLIC include)

if(MSVC)
  target_compile_options(gdpf PRIVATE /W4 /permissive-)
else()
  target_compile_options(gdpf PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

add_executable(gdpf_cli tools/gdpf_cli.cpp)
set_target_properties(gdpf_cli PROPERTIES OUTPUT_NAME gdpf)
target_link_libraries(gdpf_cli PRIVATE gdpf)

option(GDPF_BUILD_TESTS "Build the gdpf tests" ON)
if(GDPF_BUILD_TESTS)
  enable_testing()
  foreach(t codec level sim)
    add_executable(test_${t} tests/test_${t}.cpp)
    target_link_libraries(test_${t} PRIVATE gdpf)
    target_compile_definitions(test_${t} PRIVATE
      GDPF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME ${t} COMMAND test_${t})
  endforeach()
endif()
