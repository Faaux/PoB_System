# CMakeList.txt : CMake project for CMakeProject1, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_package(catch2 REQUIRED)

add_executable (tests "tests.cpp")

SET_PROJECT_WARNINGS(tests)
target_link_libraries(tests PRIVATE tasks Catch2::Catch2)

set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)
include(Catch)
catch_discover_tests(tests)