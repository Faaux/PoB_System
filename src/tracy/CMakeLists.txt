option(ENABLE_PROFILING "Enable Tracy Profiling" FALSE)

add_library(tracy STATIC TracyClient.cpp)
target_include_directories(tracy PUBLIC ${CMAKE_CURRENT_LIST_DIR})

if(ENABLE_PROFILING)
message(STATUS "Profiling is enabled.")
target_compile_definitions(tracy PUBLIC TRACY_ENABLE)
endif()