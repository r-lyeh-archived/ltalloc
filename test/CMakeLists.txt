cmake_minimum_required(VERSION 2.8)
project(ltalloc_test C CXX)

include_directories(..)

file(GLOB sources *.cpp ../ltalloc.cc)

# We don't want Catch2 to do allocations with ltalloc, so disable the operator new overrides
add_definitions(-DLTALLOC_DISABLE_OPERATOR_NEW_OVERRIDE)

add_executable(ltalloc_test ${sources})
add_test(ltalloc_test_runs ltalloc_test)
set_tests_properties (ltalloc_test_runs PROPERTIES PASS_REGULAR_EXPRESSION "RETURNCODE=0")