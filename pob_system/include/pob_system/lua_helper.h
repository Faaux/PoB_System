#pragma once
struct lua_State;

void logError( lua_State* L );

void dumpstack( lua_State* L );
void dumptable( lua_State* L, int tableIndex = 1 );