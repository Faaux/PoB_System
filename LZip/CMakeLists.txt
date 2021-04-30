cmake_minimum_required (VERSION 3.16)

# Add source to this project's executable.
add_library(lzip SHARED "lzip.cpp")

target_link_libraries(lzip PRIVATE LuaJIT::LuaJIT ZLIB::ZLIB)
install(TARGETS lzip)