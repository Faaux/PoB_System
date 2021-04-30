#include <pob_system/lua_helper.h>
#include <lua.hpp>

static void printValueAtIndex( lua_State* L, int i )
{
	switch ( lua_type( L, i ) ) {
		case LUA_TNUMBER:
			printf( "%g\n", lua_tonumber( L, i ) );
			break;
		case LUA_TSTRING:
			printf( "%s\n", lua_tostring( L, i ) );
			break;
		case LUA_TBOOLEAN:
			printf( "%s\n", ( lua_toboolean( L, i ) ? "true" : "false" ) );
			break;
		case LUA_TNIL:
			printf( "%s\n", "nil" );
			break;
		default:
			printf( "%p\n", lua_topointer( L, i ) );
			break;
	}
}

void dumpstack( lua_State* L )
{
	int top = lua_gettop( L );
	for ( int i = 1; i <= top; i++ ) {
		printf( "%d\t%s\t", i, luaL_typename( L, i ) );
		printValueAtIndex( L, i );
	}
	printf( "----- End of Stack\n" );
}

void dumptable( lua_State* L, int tableIndex )
{
	lua_pushnil( L ); /* first key */
	while ( lua_next( L, tableIndex ) != 0 ) {
		printf( "Key: %s\t", luaL_typename( L, -2 ) );
		printValueAtIndex( L, -2 );
		printf( "Value: %s\t", luaL_typename( L, -1 ) );
		printValueAtIndex( L, -1 );

		lua_pop( L, 1 );
	}
	printf( "----- End of Table\n" );
}

int traceback( lua_State* L )
{
	if ( !lua_isstring( L, 1 ) ) /* 'message' not a string? */
		return 1;				 /* keep it intact */
	lua_getglobal( L, "debug" );
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		return 1;
	}
	lua_getfield( L, -1, "traceback" );
	if ( !lua_isfunction( L, -1 ) ) {
		lua_pop( L, 2 );
		return 1;
	}
	lua_pushvalue( L, 1 );	 /* pass error message */
	lua_pushinteger( L, 2 ); /* skip this function and traceback */
	lua_call( L, 2, 1 );	 /* call debug.traceback */
	return 1;
}