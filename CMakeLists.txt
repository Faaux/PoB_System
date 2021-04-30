cmake_minimum_required (VERSION 3.15)

project ("PoBSystem")

include(cmake/CompilerWarnings.cmake)


set(CMAKE_CXX_STANDARD 20)


# Third-Party-Code
# Add luajit as lua to the project since VCPKG support is bad atm
FIND_PATH(LUA_INCLUDE_DIR luajit.h
	PATH_SUFFIXES luajit
)

FIND_LIBRARY(LUA_LIBRARY
		NAMES lua51
		PATH_SUFFIXES lib
		REQUIRED
)
get_filename_component(LUA_DLL "${LUA_LIBRARY}" PATH)
get_filename_component(LUA_DLL "${LUA_DLL}" PATH)
SET(LUA_DLL "${LUA_DLL}/bin/lua51.dll")

INCLUDE(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LUAJIT_FOUND to TRUE if
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LuaJIT
	REQUIRED_VARS LUA_LIBRARY LUA_INCLUDE_DIR LUA_DLL)

MARK_AS_ADVANCED(LUA_INCLUDE_DIR LUA_LIBRARY LUA_MATH_LIBRARY)

add_library(LuaJIT::LuaJIT SHARED IMPORTED)
set_target_properties(LuaJIT::LuaJIT PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
)

set_property(TARGET LuaJIT::LuaJIT APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(LuaJIT::LuaJIT PROPERTIES
	IMPORTED_IMPLIB_DEBUG "${LUA_LIBRARY}"
	IMPORTED_LOCATION_DEBUG "${LUA_DLL}"
	)

set_property(TARGET LuaJIT::LuaJIT APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(LuaJIT::LuaJIT PROPERTIES
	IMPORTED_IMPLIB_RELEASE "${LUA_LIBRARY}"
	IMPORTED_LOCATION_RELEASE "${LUA_DLL}"
	)


find_package(SDL2 REQUIRED) 
find_package(SDL2-image REQUIRED) 
find_package(curl REQUIRED)
find_package(ZLIB REQUIRED)

include(FetchContent)

FetchContent_Declare(
    luaCurl
    GIT_REPOSITORY https://github.com/Faaux/Lua-cURLv3.git
    GIT_TAG master
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(luaCurl)
target_link_libraries(lcurl PRIVATE CURL::libcurl LuaJIT::LuaJIT)
install(TARGETS lcurl)

# Include sub-projects.
add_subdirectory ("LZip")
add_subdirectory ("tasks")
add_subdirectory ("pob_system")
add_subdirectory ("tests")
