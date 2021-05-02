cmake_minimum_required (VERSION 3.16)

# Add source to this project's executable.
add_executable(pob_system  
	"src/main.cpp"

    "include/pob_system/lua_helper.h"
	"src/lua_helper.cpp"

	"include/pob_system/state.h"
	"src/state.cpp" "include/pob_system/image.h" "src/image.cpp"  "include/pob_system/keys.h" "src/keys.cpp"  "include/pob_system/user_path_helper.h" "src/win32.cpp")

SET_PROJECT_WARNINGS(pob_system)
target_link_libraries(pob_system PRIVATE tasks SDL2::SDL2-static SDL2::SDL2_image LuaJIT::LuaJIT)
target_include_directories(pob_system   PUBLIC include)
target_compile_definitions(pob_system   PRIVATE SDL_MAIN_HANDLED)
install(TARGETS pob_system)
format_pre_built(pob_system)