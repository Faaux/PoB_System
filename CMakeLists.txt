cmake_minimum_required (VERSION 3.15)

project ("PoBSystem")

include(cmake/CompilerWarnings.cmake)

set(CMAKE_CXX_STANDARD 20)

# Third-Party-Code
find_package(SDL2 REQUIRED) 
find_package(Lua REQUIRED)

# Include sub-projects.
add_subdirectory ("tasks")
add_subdirectory ("pob_system")
add_subdirectory ("tests")
