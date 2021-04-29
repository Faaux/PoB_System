# CMakeList.txt : CMake project for CMakeProject1, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_package(catch2 REQUIRED)

# Add source to this project's executable.
add_executable (tests "tests.cpp")

SET_PROJECT_WARNINGS(tests)
target_link_libraries(tests PRIVATE tasks Catch2::Catch2)
