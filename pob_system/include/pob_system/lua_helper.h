#pragma once
#include <vector>
#include <string>

struct lua_State;

void logError( lua_State* L );

void dumpstack( lua_State* L );
void dumptable( lua_State* L, int tableIndex = 1 );
int traceback( lua_State* L );

struct lua_value
{
	~lua_value() = default;
	enum
	{
		NIL,
		BOOLEAN,
		NUMBER,
		STRING
	} type
		= NIL;

	bool b = false;
	double n = -1;
	std::string s;
};

std::vector< lua_value > pop_save_values( lua_State* l, int start );

void push_saved_values( lua_State* l, const std::vector< lua_value >& values );