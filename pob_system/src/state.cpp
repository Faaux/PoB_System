#include <pob_system/state.h>
#include <pob_system/lua_helper.h>

#include <lua.hpp>
#include <SDL.h>

#include <chrono>
#include <filesystem>

lua_state_t::lua_state_t( state_t* state ) : state( state )
{
	l = luaL_newstate();

	luaL_openlibs( l );

	{
		lua_getglobal( l, "package" );
		lua_getfield( l, -1, "path" );				   // get field "path" from table at top of stack (-1)
		std::string cur_path = lua_tostring( l, -1 );  // grab path string from top of stack
		auto cwd = std::filesystem::current_path().string();
		cur_path.append( ";" + cwd + "\\lua\\?.lua" );
		cur_path.append( ";" + cwd + "\\lua\\?\\init.lua" );
		cur_path.append( ";" + cwd + "?.lua" );
		cur_path.append( ";" + cwd + "?\\init.lua" );
		lua_pop( l, 1 );						// get rid of the string on the stack we just pushed on line 5
		lua_pushstring( l, cur_path.c_str() );	// push the new one
		lua_setfield( l, -2, "path" );	// set the field "path" in table at -2 with value at top of stack
		lua_pop( l, 1 );				// get rid of package table from top of stack
	}

	// Register callbacks
	lua_pushcfunction(
		l, []( lua_State* l ) -> int { return state_t::instance->lua_state.set_window_title(); } );
	lua_setglobal( l, "SetWindowTitle" );

	lua_pushcfunction( l,
					   []( lua_State* l ) -> int { return state_t::instance->lua_state.set_main_object(); } );
	lua_setglobal( l, "SetMainObject" );

	lua_pushcfunction( l, []( lua_State* l ) -> int { return state_t::instance->lua_state.get_time(); } );
	lua_setglobal( l, "GetTime" );

	lua_pushcfunction( l, []( lua_State* l ) -> int { return state_t::instance->lua_state.render_init(); } );
	lua_setglobal( l, "RenderInit" );

	lua_pushcfunction( l, []( lua_State* l ) -> int { return state_t::instance->lua_state.con_print_f(); } );
	lua_setglobal( l, "ConPrintf" );

	lua_pushcfunction( l, []( lua_State* l ) -> int { return state_t::instance->lua_state.load_module(); } );
	lua_setglobal( l, "LoadModule" );

	lua_pushcfunction( l,
					   []( lua_State* l ) -> int { return state_t::instance->lua_state.p_load_module(); } );
	lua_setglobal( l, "PLoadModule" );

	lua_pushcfunction( l, []( lua_State* l ) -> int { return state_t::instance->lua_state.p_call(); } );
	lua_setglobal( l, "PCall" );

	lua_pushcfunction(
		l, []( lua_State* l ) -> int { return state_t::instance->lua_state.launch_sub_script(); } );
	lua_setglobal( l, "LaunchSubScript" );

#define STUB( n )                                     \
	lua_pushcfunction( l, []( lua_State* l ) -> int { \
		printf( n "\n" );                             \
		return 0;                                     \
	} );                                              \
	lua_setglobal( l, n );

	STUB( "SetDrawLayer" );
	STUB( "SetViewport" );
	STUB( "GetScreenSize" );
	STUB( "SetDrawColor" );
	STUB( "DrawImage" );
	STUB( "DrawStringWidth" );

#undef STUB
}

lua_state_t::~lua_state_t()
{
	lua_close( l );
}

void lua_state_t::do_file( const char* file )
{
	if ( luaL_dofile( l, file ) != LUA_OK ) {
		logLuaError();
	}
}

void lua_state_t::onInit()
{
	callParameterlessFunction( "OnInit" );
}

void lua_state_t::onFrame()
{
	callParameterlessFunction( "OnFrame" );
}

void lua_state_t::assert( bool cond, const char* fmt, ... ) const
{
	if ( !cond ) {
		va_list va;
		va_start( va, fmt );
		lua_pushvfstring( l, fmt, va );
		va_end( va );
		lua_error( l );
	}
}

int lua_state_t::set_window_title()
{
	luaL_checktype( l, 1, LUA_TSTRING );
	if ( !state ) {
		fprintf( stderr, "Can not set window_state with no valid state" );
		return 0;
	}
	state->render_state.window_title = lua_tostring( l, 1 );
	return 0;
}

int lua_state_t::set_main_object()
{
	luaL_checktype( l, 1, LUA_TTABLE );

	// Create table in registry
	lua_pushlightuserdata( l, (void*)this ); /* push address */
	lua_newtable( l );
	lua_settable( l, LUA_REGISTRYINDEX );

	// Read table from registry
	lua_pushlightuserdata( l, (void*)this ); /* push address */
	lua_gettable( l, LUA_REGISTRYINDEX );

	// Swap stack so that the table is at 1 and argument at 2
	lua_insert( l, 1 );
	main_object_index = luaL_ref( l, 1 );
	lua_pop( l, 1 );
	return 0;
}

int lua_state_t::get_time()
{
	auto ms = std::chrono::duration_cast< std::chrono::milliseconds >(
		std::chrono::high_resolution_clock::now().time_since_epoch() );
	lua_pushnumber( l, static_cast< double >( ms.count() ) );
	return 1;
}

int lua_state_t::render_init()
{
	if ( !state ) {
		fprintf( stderr, "Can not create rendersystem with no valid state" );
		return 0;
	}
	state->render_state.init();
	return 0;
}

int lua_state_t::con_print_f()
{
	int n = lua_gettop( l );

	lua_getglobal( l, "string" );
	lua_getfield( l, -1, "format" );
	lua_insert( l, 1 );
	lua_pop( l, 1 );

	// Call on frame
	if ( lua_pcall( l, n, 1, 0 ) != LUA_OK ) {
		logLuaError();
		lua_settop( l, 0 );
		return 0;
	}

	printf( "%s\n", luaL_checkstring( l, 1 ) );
	lua_settop( l, 0 );

	return 0;
}

int lua_state_t::load_module()
{
	int n = lua_gettop( l );
	assert( n >= 1 && lua_isstring( l, 1 ), "LoadModule wrong call." );

	std::string fileName( lua_tostring( l, 1 ) );
	constexpr std::string_view ext( ".lua" );
	if ( !std::equal( ext.rbegin(), ext.rend(), fileName.rbegin() ) ) {
		fileName = fileName + ".lua";
	}

	int err = luaL_loadfile( l, fileName.c_str() );
	assert( err == 0, "LoadModule() error loading '%s':\n%s", fileName.c_str(), lua_tostring( l, -1 ) );
	lua_replace( l, 1 );  // Replace module name with module main chunk
	lua_call( l, n - 1, LUA_MULTRET );
	return lua_gettop( l );
}

int lua_state_t::p_load_module()
{
	int n = lua_gettop( l );
	assert( n >= 1, "Usage: PLoadModule(name[, ...])" );
	assert( lua_isstring( l, 1 ), "PLoadModule() argument 1: expected string, got %t", 1 );
	std::string fileName( lua_tostring( l, 1 ) );
	constexpr std::string_view ext( ".lua" );
	if ( !std::equal( ext.rbegin(), ext.rend(), fileName.rbegin() ) ) {
		fileName = fileName + ".lua";
	}
	int err = luaL_loadfile( l, fileName.c_str() );
	if ( err ) {
		return 1;
	}
	lua_replace( l, 1 );  // Replace module name with module main chunk
	err = lua_pcall( l, n - 1, LUA_MULTRET, 0 );
	if ( err ) {
		return 1;
	}
	lua_pushnil( l );
	lua_insert( l, 1 );
	//    lua_replace(L, 1); // Replace traceback function with nil
	return lua_gettop( l );
}

int lua_state_t::p_call()
{
	int n = lua_gettop( l );
	assert( n >= 1, "Usage: PCall(func[, ...])" );
	assert( lua_isfunction( l, 1 ), "PCall() argument 1: expected function, got %t", 1 );
	dumpstack( l );
	lua_getfield( l, LUA_REGISTRYINDEX, "traceback" );
	lua_insert( l, 1 );	 // Insert traceback function at start of stack
	int err = lua_pcall( l, n - 1, LUA_MULTRET, 1 );
	if ( err ) {
		lua_error( l );
		return 1;
	}
	lua_pushnil( l );
	lua_replace( l, 1 );  // Replace traceback function with nil
	return lua_gettop( l );
}

int lua_state_t::launch_sub_script()
{
	// TODO: Put this on a different thread
	// TODO: return id
	// TODO: Forward calls from third parameter to OnSubCall
	// TODO: Call OnSubError
	// TODO: Call OnSubFinished

	int n = lua_gettop( l );
	int argsCount = n - 3;
	auto& sub = sub_programs.emplace_back( nullptr );

	const char* script = luaL_checkstring( l, 1 );
	if ( script[ 0 ] == '#' && script[ 1 ] == '@' ) {
		script = script + 2;
	}

	luaL_loadstring( sub.l, script );
	lua_xmove( l, sub.l, argsCount );

	if ( lua_pcall( sub.l, argsCount, LUA_MULTRET, 0 ) != LUA_OK ) {
		dumpstack( sub.l );
		sub.logLuaError();
	}

	return 0;
}

int lua_state_t::dummy()
{
	dumpstack( l );
	return 0;
}

void lua_state_t::pushMainObjectOntoStack()
{
	lua_pushlightuserdata( l, (void*)this );
	lua_gettable( l, LUA_REGISTRYINDEX );

	// Read Main object table
	lua_pushinteger( l, main_object_index );
	lua_gettable( l, -2 );
	lua_remove( l, -2 );
}

void lua_state_t::pushCallableOntoStack( const char* name )
{
	pushMainObjectOntoStack();

	// Get function
	lua_pushstring( l, name );
	lua_gettable( l, -2 );

	// Table has to be first argument
	lua_insert( l, -2 );
}

void lua_state_t::callParameterlessFunction( const char* name )
{
	pushCallableOntoStack( name );

	// Call on frame
	if ( lua_pcall( l, 1, 0, 0 ) != LUA_OK ) {
		logLuaError();
		lua_settop( l, 0 );
		return;
	}

	// clear stack
	lua_settop( l, 0 );
}

void lua_state_t::logLuaError()
{
	fprintf( stderr, "%s\n", lua_tostring( l, -1 ) );
}

render_state_t::~render_state_t()
{
	if ( renderer ) {
		SDL_DestroyRenderer( renderer );
	}
	if ( window ) {
		SDL_DestroyWindow( window );
	}
}

void render_state_t::init()
{
	if ( is_init ) return;
	// TODO: Safe and load position size etc.

	window = SDL_CreateWindow( window_title.c_str(),
							   SDL_WINDOWPOS_UNDEFINED,	 // initial x position
							   SDL_WINDOWPOS_UNDEFINED,	 // initial y position
							   640,						 // width, in pixels
							   480,						 // height, in pixels
							   SDL_WINDOW_OPENGL		 // flags - see below
	);

	renderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
	if ( renderer == nullptr ) {
		fprintf( stderr, "Could not create a renderer: %s", SDL_GetError() );
	}

	is_init = true;
}

state_t* state_t::instance = nullptr;
