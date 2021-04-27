cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable(pob_system  
	"src/main.cpp"
   "include/pob_system/lua_helper.h" "src/lua_helper.cpp" "include/pob_system/state.h" "src/state.cpp")

SET_PROJECT_WARNINGS(pob_system)
target_link_libraries(pob_system PRIVATE SDL2::SDL2-static ${LUA_LIBRARIES})
target_include_directories(pob_system   PUBLIC include
                                        PRIVATE ${LUA_INCLUDE_DIR})