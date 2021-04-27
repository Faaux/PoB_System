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