cmake_minimum_required(VERSION 2.8)
project(ltalloc_test C CXX)
include(CTest)

include_directories(..)

file(GLOB sources *.cpp ../ltalloc.cc)

# Ltalloc doesn't need c++11 but the test project does.
set(CMAKE_CXX_STANDARD 11)

add_executable(ltalloc_test ${sources})

find_package(Threads REQUIRED)
target_link_libraries(ltalloc_test Threads::Threads)

# We don't want Catch2 to do allocations with ltalloc, so disable the operator new overrides.
add_definitions(-DLTALLOC_DISABLE_OPERATOR_NEW_OVERRIDE)

add_test(ltalloc_test_runs ltalloc_test)