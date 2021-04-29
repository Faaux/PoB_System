#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <tasks/task.h>
#include <tasks/static_thread_pool.h>

// Forward Declare
struct lua_State;
struct SDL_Window;
struct SDL_Renderer;

class state_t;

// All lua specific state with a pointer to application state
// Is also the home for all API callbacks
class lua_state_t
{
public:
	lua_state_t( state_t* state );
	~lua_state_t();

	void do_file( const char* file );

	void checkSubPrograms();
	int get_id() const
	{
		return id;
	}

	// Helpers to call into lua
	void onInit();
	void onFrame();

private:
	void assert( bool cond, const char* fmt, ... ) const;

	// Callbacks
	int set_window_title();
	int set_main_object();
	int get_time();
	int render_init();
	int con_print_f();
	int load_module();
	int p_load_module();
	int p_call();
	int launch_sub_script();
	int dummy();

	// Internal Helper to access the main lua table
	void pushMainObjectOntoStack();
	void pushCallableOntoStack( const char* name );
	void callParameterlessFunction( const char* name );
	void logLuaError();

	int id;
	state_t* state;
	lua_State* l;
	std::vector< cb::task< std::shared_ptr< lua_state_t >, true > > sub_programs;
	int main_object_index = -1;
};

struct render_state_t
{
	~render_state_t();

	void init();
	bool is_init = false;
	std::string window_title;
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
};

// The global state of the application.
// For testing we do not create a singleton, we do however allocate an instance on startup and make it
// accessible via a static var
class state_t
{
public:
	// No getters and setters, know what you change
	lua_state_t lua_state{ this };
	render_state_t render_state;
	cb::static_thread_pool tp;

	static state_t* instance;
};